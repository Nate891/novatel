#include "novatel/novatel.h"
#include <iostream>
#include <valarray>
#include <fstream>
#include <sstream>

using namespace std;
using namespace novatel;

/////////////////////////////////////////////////////
// includes for default time callback
#define WIN32_LEAN_AND_MEAN
#include "boost/date_time/posix_time/posix_time.hpp"
////////////////////////////////////////////////////


/* --------------------------------------------------------------------------
Calculate a CRC value to be used by CRC calculation functions.
-------------------------------------------------------------------------- */
unsigned long CRC32Value(int i)
{
  int j;
  unsigned long ulCRC;
  ulCRC = i;
  for ( j = 8 ; j > 0; j-- ) {
    if ( ulCRC & 1 )
      ulCRC = ( ulCRC >> 1 ) ^ CRC32_POLYNOMIAL;
    else
      ulCRC >>= 1;
  }
    return ulCRC;
}


/* --------------------------------------------------------------------------
Calculates the CRC-32 of a block of data all at once
-------------------------------------------------------------------------- */
unsigned long CalculateBlockCRC32 ( unsigned long ulCount, /* Number of bytes in the data block */
                                    unsigned char *ucBuffer ) /* Data block */
{
  unsigned long ulTemp1;
  unsigned long ulTemp2;
  unsigned long ulCRC = 0;
  while ( ulCount-- != 0 ) {
    ulTemp1 = ( ulCRC >> 8 ) & 0x00FFFFFFL;
    ulTemp2 = CRC32Value( ((int) ulCRC ^ *ucBuffer++ ) & 0xff );
    ulCRC = ulTemp1 ^ ulTemp2;
  }
  return( ulCRC );
}





/*!
 * Default callback method for timestamping data.  Used if a
 * user callback is not set.  Returns the current time from the
 * CPU clock as the number of seconds from Jan 1, 1970
 */
double DefaultGetTime() {
	boost::posix_time::ptime present_time(boost::posix_time::microsec_clock::universal_time());
	boost::posix_time::time_duration duration(present_time.time_of_day());
	return (double)(duration.total_milliseconds())/1000.0;
}




inline void printHex(char *data, int length) {
  for (int i = 0; i < length; ++i) {
    printf("0x%X ", (unsigned) (unsigned char) data[i]);
  }
  printf("\n");
}


// stolen from: http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ") {
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos)
	{
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}

void DefaultAcknowledgementHandler() {
    ;//std::cout << "Acknowledgement received." << std::endl;
}

inline void DefaultDebugMsgCallback(const std::string &msg) {
    ;//std::cout << "Novatel Debug: " << msg << std::endl;
}

inline void DefaultInfoMsgCallback(const std::string &msg) {
    std::cout << "Novatel Info: " << msg << std::endl;
}

inline void DefaultWarningMsgCallback(const std::string &msg) {
    std::cout << "Novatel Warning: " << msg << std::endl;
}

inline void DefaultErrorMsgCallback(const std::string &msg) {
    std::cout << "Novatel Error: " << msg << std::endl;
}

void DefaultBestPositionCallback(Position best_position, double time_stamp){
    std:: cout << "BESTPOS: \nGPS Week: " << best_position.header.gps_week <<
                  "  GPS milliseconds: " << best_position.header.gps_millisecs << std::endl <<
                  "  Latitude: " << best_position.latitude << std::endl <<
                  "  Longitude: " << best_position.longitude << std::endl <<
                  "  Height: " << best_position.height << std::endl << std::endl;
}

Novatel::Novatel() {
	serial_port_=NULL;
	reading_status_=false;
	time_handler_ = DefaultGetTime;
    handle_acknowledgement_=DefaultAcknowledgementHandler;
    best_position_callback_=DefaultBestPositionCallback;
    log_debug_=DefaultDebugMsgCallback;
    log_info_=DefaultInfoMsgCallback;
    log_warning_=DefaultWarningMsgCallback;
    log_error_=DefaultErrorMsgCallback;
	reading_acknowledgement_=false;
    buffer_index_=0;
    read_timestamp_=0;
    parse_timestamp_=0;
    ack_received_=false;
    is_connected_ = false;
}

Novatel::~Novatel() {
    Disconnect();
}

bool Novatel::Connect(std::string port, int baudrate, bool search) {

	bool connected = Connect_(port, baudrate);

	if (!connected && search) {
		// search additional baud rates
		int bauds_to_search[5]={9600,19200,38400,57600,115200};
		bool found = false;
		for (int ii=0; ii<5; ii++){
			std::stringstream search_msg;
			search_msg << "Searching for receiver with baudrate: " << bauds_to_search[ii];
			log_info_(search_msg.str());
			if (Connect_(port, bauds_to_search[ii])) {
				found = true;
				break;
			}
		}

		// if the receiver was found on a different baud rate, 
		// change its setting to the selected baud rate and reconnect
		if (found) {
			// change baud rate to selected value
			std::stringstream cmd;
			cmd << "COM " << baudrate << "\r\n";
			std::stringstream baud_msg;
			baud_msg << "Changing receiver baud rate to " << baudrate;
			log_info_(baud_msg.str());
			try {
				serial_port_->write(cmd.str());
			} catch (std::exception &e) {
				std::stringstream output;
			    output << "Error changing baud rate: " << e.what();
			    log_error_(output.str());
			    return false;
			}
			Disconnect();
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
			connected = Connect_(port, baudrate);
		} 
	}

	if (connected) {
		// start reading
		StartReading();
		is_connected_ = true;
		return true;
	} else {
		log_error_("Failed to connect.");
		return false;
	}

}

bool Novatel::Connect_(std::string port, int baudrate=115200) {
	try {

		//serial::Timeout my_timeout(50, 200, 0, 200, 0); // 115200 working settings
		//serial_port_ = new serial::Serial(port,baudrate,my_timeout);

		serial_port_ = new serial::Serial(port,baudrate,serial::Timeout::simpleTimeout(50));

		if (!serial_port_->isOpen()){
	        std::stringstream output;
	        output << "Serial port: " << port << " failed to open." << std::endl;
	        log_error_(output.str());
			delete serial_port_;
			serial_port_ = NULL;
			return false;
		} else {
	        std::stringstream output;
	        output << "Serial port: " << port << " opened successfully." << std::endl;
	        log_info_(output.str());
		}

		// stop any incoming data and flush buffers
		serial_port_->write("UNLOGALL\r\n");
		// wait for data to stop cominig in
		boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
		// clear serial port buffers
		serial_port_->flush();

		// look for GPS by sending ping and waiting for response
		if (!Ping()){
	        std::stringstream output;
	        output << "Novatel GPS not found on port: " << port << " at baudrate " << baudrate << std::endl;
	        log_error_(output.str());
			delete serial_port_;
			serial_port_ = NULL;
			is_connected_ = false;
			return false;
		}
	} catch (std::exception &e) {
	    std::stringstream output;
	    output << "Error connecting to gps on com port " << port << ": " << e.what();
	    log_error_(output.str());
	    is_connected_ = false;
	    return false;
	}

	return true;
}


void Novatel::Disconnect() {
	log_info_("Novatel disconnecting.");
	StopReading();
	// sleep longer than the timeout period
	boost::this_thread::sleep(boost::posix_time::milliseconds(150));

	try {
		if ((serial_port_!=NULL) && (serial_port_->isOpen()) ) {
			log_info_("Sending UNLOGALL and closing port.");
			serial_port_->write("UNLOGALL\r\n");
			serial_port_->close();
			delete serial_port_;
			serial_port_=NULL;
		}
	} catch (std::exception &e) {
	    std::stringstream output;
	    output << "Error during disconnect: " << e.what();
	    log_error_(output.str());
	}
}

bool Novatel::Ping(int num_attempts) {

	while ((num_attempts--)>0) {
        std::stringstream output;
        output << "Searching for Novatel receiver..." << std::endl;
        log_info_(output.str());
		if (UpdateVersion()) {
            std::stringstream output;
            output << "Found Novatel receiver." << std::endl;
            output << "\tModel: " << model_ << std::endl;
            output << "\tSerial Number: " << serial_number_ << std::endl;
            output << "\tHardware version: " << hardware_version_ << std::endl;
            output << "\tSoftware version: " << software_version_ << std::endl << std::endl;;
            output << "Receiver capabilities:" << std::endl;
            output << "\tL2: ";
			if (l2_capable_)
                output << "+" << std::endl;
			else
                output << "-" << std::endl;
            output << "\tRaw measurements: ";
			if (raw_capable_)
                output << "+" << std::endl;
			else
                output << "-" << std::endl;
            output << "\tRTK: ";
			if (rtk_capable_)
                output << "+" << std::endl;
			else
                output << "-" << std::endl;
            output << "\tSPAN: ";
			if (span_capable_)
                output << "+" << std::endl;
			else
                output << "-" << std::endl;
            output << "\tGLONASS: ";
			if (glonass_capable_)
                output << "+" << std::endl;
			else
                output << "-" << std::endl;
            log_info_(output.str());
            return true;
		}
	}

	// no response found
	return false;

}

bool Novatel::SendCommand(std::string cmd_msg) {
	try {
		// sends command to GPS receiver
        serial_port_->write(cmd_msg + "\r\n");
		// wait for acknowledgement (or 2 seconds)
		boost::mutex::scoped_lock lock(ack_mutex_);
		boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(2000);
		if (ack_condition_.timed_wait(lock,timeout)) {
      log_info_("Command `" + cmd_msg + "` sent to GPS receiver.");
			return true;
		} else {
            log_error_("Command '" + cmd_msg + "' failed.");
			return false;
		}
	} catch (std::exception &e) {
		std::stringstream output;
        output << "Error in Novatel::SendCommand(): " << e.what();
        log_error_(output.str());
        return false;
	}
}

bool Novatel::SetSvElevationAngleCutoff(float angle) {
    try {
        std::stringstream ang_cmd;
        ang_cmd << "ECUTOFF " << angle;
        return SendCommand(ang_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::SetSvElevationCutoff(): " << e.what();
        log_error_(output.str());
        return false;
    }
}

void Novatel::PDPFilterDisable() {
    try{
    std::stringstream pdp_cmd;
    pdp_cmd << "PDPFILTER DISABLE" ;
    bool result = SendCommand(pdp_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::PDPFilterDisable(): " << e.what();
        log_error_(output.str());
    }
}

void Novatel::PDPFilterEnable() {
    try{
    std::stringstream pdp_cmd;
    pdp_cmd << "PDPFILTER ENABLE" ;
    bool result = SendCommand(pdp_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::PDPFilterEnable(): " << e.what();
        log_error_(output.str());
    }
}

void Novatel::PDPFilterReset() {
    try{
    std::stringstream pdp_cmd;
    pdp_cmd << "PDPFILTER RESET";
    bool result = SendCommand(pdp_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::PDPFilterReset(): " << e.what();
        log_error_(output.str());
    }
}

//! TODO: PROPAK DOESN"T ACCEPT, LIKES REV.1 PASSTOPASSMODE INSTEAD
void Novatel::PDPModeConfigure(PDPMode mode, PDPDynamics dynamics) {
    try {
        std::stringstream pdp_cmd;

        pdp_cmd << "PDPMODE ";
        if (mode == NORMAL)
            pdp_cmd << "NORMAL ";
        else if (mode == RELATIVE)
            pdp_cmd << "RELATIVE ";
        else {
            log_error_("PDPModeConfigure() input 'mode'' is not valid!");
            return;
        }
        if (dynamics == AUTO)
            pdp_cmd << "AUTO";
        else if (dynamics == STATIC)
            pdp_cmd << "STATIC";
        else if (dynamics == DYNAMIC)
            pdp_cmd << "DYNAMIC";
        else {
            log_error_("PDPModeConfigure() input 'dynamics' is not valid!");
            return;
        }

        bool result = SendCommand(pdp_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::PDPModeConfigure(): " << e.what();
        log_error_(output.str());
    }
}

void Novatel::SetPositionTimeout(uint32_t seconds){
    try {
        if(0<=seconds<=86400) {
            std::stringstream pdp_cmd;
            pdp_cmd << "POSTIMEOUT " << seconds;
            bool result = SendCommand(pdp_cmd.str());
        } else
            log_error_("Seconds is not a valid value!");
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::SetPositionTimeout(): " << e.what();
        log_error_(output.str());
    }
}

bool Novatel::SetInitialPosition(double latitude, double longitude, double height) {
    std::stringstream pos_cmd;
    pos_cmd << "SETAPPROXPOS " << latitude << " " << longitude << " " << height;
    return SendCommand(pos_cmd.str());

}

bool Novatel::SetInitialTime(uint32_t gps_week, double gps_seconds) {
    std::stringstream time_cmd;
    time_cmd << "SETAPPROXTIME " << gps_week << " " << gps_seconds;
    return SendCommand(time_cmd.str());
}

bool Novatel::SetCarrierSmoothing(uint32_t l1_time_constant, uint32_t l2_time_constant) {
    try {
        std::stringstream smooth_cmd;
        if ((2 >= l1_time_constant) || (l1_time_constant >= 2000)) {
            log_error_("Error in SetCarrierSmoothing: l1_time_constant set to improper value.");
            return false;
        } else if ((5 >= l2_time_constant) || (l2_time_constant >= 2000)) {
            log_error_("Error in SetCarrierSmoothing: l2_time_constant set to improper value.");
            return false;
        } else {
            smooth_cmd << "CSMOOTH " << l1_time_constant << " " << l2_time_constant;
        }
        return SendCommand(smooth_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::SetCarrierSmoothing(): " << e.what();
        log_error_(output.str());
        return false;
    }
}

bool Novatel::HardwareReset(uint8_t rst_delay) {
    // Resets receiver to cold start, does NOT clear non-volatile memory!
    try {
        std::stringstream rst_cmd;
        rst_cmd << "RESET " << rst_delay;
        return SendCommand(rst_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::HardwareReset(): " << e.what();
        log_error_(output.str());
        return false;
    }
}

bool Novatel::HotStartReset() {
    try {
        std::stringstream rst_cmd;
        rst_cmd << "FRESET " << LAST_POSITION;
        return SendCommand(rst_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::HotStartReset(): " << e.what();
        log_error_(output.str());
        return false;
    }
}

bool Novatel::WarmStartReset() {
    try {
        std::stringstream rst_pos_cmd;
        std::stringstream rst_time_cmd;
        rst_pos_cmd << "FRESET " << LAST_POSITION;
        bool pos_reset = SendCommand(rst_pos_cmd.str());
        rst_time_cmd << "FRESET " << LBAND_TCXO_OFFSET ;
        bool time_reset = SendCommand(rst_time_cmd.str());
        return (pos_reset && time_reset);
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::WarmStartReset(): " << e.what();
        log_error_(output.str());
        return false;
    }
}

bool Novatel::ColdStartReset() {
    try {
        std::stringstream rst_cmd;
        rst_cmd << "FRESET " << STANDARD;
        return SendCommand(rst_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::ColdStartReset(): " << e.what();
        log_error_(output.str());
        return false;
    }
}

void Novatel::SaveConfiguration() {
    try {
        bool result = SendCommand("SAVECONFIG");
        if(result)
            log_info_("Receiver configuration has been saved.");
        else
            log_error_("Failed to save receiver configuration!");
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::SaveConfiguration(): " << e.what();
        log_error_(output.str());
    }
}

void Novatel::ConfigureLogs(std::string log_string) {
	// parse log_string on semicolons (;)
	std::vector<std::string> logs;

	Tokenize(log_string, logs, ";");

	// request each log from the receiver and wait for an ack
	for (std::vector<std::string>::iterator it = logs.begin() ; it != logs.end(); ++it)
	{
		// try each command up to five times
		int ii=0;
		while (ii<5) {
			try {
				// send log command to gps (e.g. "LOG BESTUTMB ONTIME 1.0")
				serial_port_->write("LOG " + *it + "\r\n");
				std::stringstream cmd;
				cmd << "LOG " << *it << "\r\n";
				log_info_(cmd.str());
				// wait for acknowledgement (or 2 seconds)
				boost::mutex::scoped_lock lock(ack_mutex_);
				boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(2000);
				if (ack_condition_.timed_wait(lock,timeout)) {
					log_info_("Ack received for requested log: " + *it);
					break;
				} else {
					log_error_("No acknowledgement received for log: " + *it);
				}
			} catch (std::exception &e) {
				std::stringstream output;
		        output << "Error configuring receiver logs: " << e.what();
		        log_error_(output.str());
			}
		}

	}

}

void Novatel::Unlog(std::string log) {
    try {
        std::stringstream unlog_cmd;
        unlog_cmd << "UNLOG " << log;
        bool result = SendCommand(unlog_cmd.str());
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::Unlog(): " << e.what();
        log_error_(output.str());
    }
}

void Novatel::UnlogAll() {
    try {
        bool result = SendCommand("UNLOGALL");
    } catch (std::exception &e) {
        std::stringstream output;
        output << "Error in Novatel::UnlogAll(): " << e.what();
        log_error_(output.str());
    }
}

void Novatel::ConfigureInterfaceMode(std::string com_port,  
  std::string rx_mode, std::string tx_mode) {

	try {
		// send command to set interface mode on com port
		// ex: INTERFACEMODE COM2 RX_MODE TX_MODE
		serial_port_->write("INTERFACEMODE " + com_port + " " + rx_mode + " " + tx_mode + "\r\n");
		// wait for acknowledgement (or 2 seconds)
		boost::mutex::scoped_lock lock(ack_mutex_);
		boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(2000);
		if (ack_condition_.timed_wait(lock,timeout)) {
			log_info_("Ack received.  Interface mode for port " + 
				com_port + " set to: " + rx_mode + " " + tx_mode);
		} else {
			log_error_("No acknowledgement received for interface mode command.");
		}
	} catch (std::exception &e) {
		std::stringstream output;
        output << "Error configuring interface mode: " << e.what();
        log_error_(output.str());
	}
}

void Novatel::ConfigureBaudRate(std::string com_port, int baudrate) {
	try {
		// send command to set baud rate on GPS com port
		// ex: COM com1 9600 n 8 1 n off on
		std::stringstream cmd;
		cmd << "COM " << com_port << " " << baudrate << " n 8 1 n off on\r\n";
		serial_port_->write(cmd.str());
		// wait for acknowledgement (or 2 seconds)
		boost::mutex::scoped_lock lock(ack_mutex_);
		boost::system_time const timeout=boost::get_system_time()+ boost::posix_time::milliseconds(2000);
		if (ack_condition_.timed_wait(lock,timeout)) {
			std::stringstream log_out;
			log_out << "Ack received.  Baud rate on com port " <<
				com_port << " set to " << baudrate << std::endl;
			log_info_(log_out.str());
		} else {
			log_error_("No acknowledgement received for com configure command.");
		}
	} catch (std::exception &e) {
		std::stringstream output;
        output << "Error configuring baud rate: " << e.what();
        log_error_(output.str());
	}
}

bool Novatel::UpdateVersion()
{
	// request the receiver version and wait for a response
	// example response:
	//#VERSIONA,COM1,0,71.5,FINESTEERING,1362,340308.478,00000008,3681,2291;
	//    1,GPSCARD,"L12RV","DZZ06040010","OEMV2G-2.00-2T","3.000A19","3.000A9",
	//    "2006/Feb/ 9","17:14:33"*5e8df6e0

	try {
		// clear port
		serial_port_->flush();
		// read out any data currently in the buffer
		std::string read_data = serial_port_->read(5000);
		while (read_data.length())
			read_data = serial_port_->read(5000);

		// send request for version
		serial_port_->write("log versiona once\r\n");
		// wait for response from the receiver
		boost::this_thread::sleep(boost::posix_time::milliseconds(500));
		// read from the serial port until a new line character is seen
		std::string gps_response = serial_port_->read(15000);

		std::vector<std::string> packets;

		Tokenize(gps_response, packets, "\n");

		// loop through all packets in file and check for version messages
		// stop when the first is found or all packets are read
		for (size_t ii=0; ii<packets.size(); ii++) {
			if (ParseVersion(packets[ii])) {
				return true;
			}
		} 
	} catch (std::exception &e) {
        std::stringstream output;
        output << "Error reading version info from receiver: " << e.what();
        log_error_(output.str());
        return false;
    }


	return false;

}

bool Novatel::ParseVersion(std::string packet) {
	// parse the results - message should start with "#VERSIONA"
        size_t found_version=packet.find("VERSIONA");
		if (found_version==string::npos)
			return false;

		// parse version information
		// remove header
		size_t pos=packet.find(";");
		if (pos==string::npos) {
            log_error_("Error parsing received version."
                       " End of message was not found");
            log_debug_(packet);
			return false;
		}

		// remove header from message
		std::string message=packet.substr(pos+1, packet.length()-pos-2);
		// parse message body by tokening on ","
		typedef boost::tokenizer<boost::char_separator<char> >
			tokenizer;
		boost::char_separator<char> sep(",");
		tokenizer tokens(message, sep);
		// set up iterator to go through token list
		tokenizer::iterator current_token=tokens.begin();
		string num_comps_string=*(current_token);
		int number_components=atoi(num_comps_string.c_str());
		// make sure the correct number of tokens were found
		int token_count=0;
		for(current_token=tokens.begin(); current_token!=tokens.end();++current_token)
		{
			//log_debug_(*current_token);
			token_count++;
		}

		// should be 9 tokens, if not something is wrong
		if (token_count!=(8*number_components+1)) {
            log_error_("Error parsing received version. "
                       "Incorrect number of tokens found.");
            std::stringstream err_out;
			err_out << "Found: " << token_count << "  Expected: " << (8*number_components+1);
			log_error_(err_out.str());
			log_debug_(packet);
			return false;
		}

		current_token=tokens.begin();
		// device type is 2nd token
		string device_type=*(++current_token);
		// model is 3rd token
		model_=*(++current_token);
		// serial number is 4th token
		serial_number_=*(++current_token);
		// model is 5rd token
		hardware_version_=*(++current_token);
		// model is 6rd token
		software_version_=*(++current_token);

		// parse the version:
		if (hardware_version_.length()>3)
            protocol_version_=hardware_version_.substr(1,4);
		else
			protocol_version_="UNKNOWN";

		// parse model number:
		// is the receiver capable of raw measurements?
        if (model_.find("L")!=string::npos)
			raw_capable_=true;
		else
			raw_capable_=false;

		// can the receiver receive L2?
		if (model_.find("12")!=string::npos)
			l2_capable_=true;
		else
			l2_capable_=false;

		// can the receiver receive GLONASS?
		if (model_.find("G")!=string::npos)
			glonass_capable_=true;
		else
			glonass_capable_=false;

		// Is this a SPAN unit?
		if ((model_.find("I")!=string::npos)||(model_.find("J")!=string::npos))
			span_capable_=true;
		else
			span_capable_=false;

		// Can the receiver process RTK?
		if (model_.find("R")!=string::npos)
			rtk_capable_=true;
		else
			rtk_capable_=false;


        // fix for oem4 span receivers - do not use l12 notation
        // i think all oem4 spans are l1 l2 capable and raw capable
        if ((protocol_version_=="OEM4")&&(span_capable_)) {
            l2_capable_=true;
            raw_capable_=true;
        }

		return true;

}

void Novatel::StartReading() {
	if (reading_status_)
		return;
	// create thread to read from sensor
	reading_status_=true;
	read_thread_ptr_ = boost::shared_ptr<boost::thread >
		(new boost::thread(boost::bind(&Novatel::ReadSerialPort, this)));
}

void Novatel::StopReading() {
	reading_status_=false;
}

void Novatel::ReadSerialPort() {
	unsigned char buffer[MAX_NOUT_SIZE];
	size_t len;
	log_info_("Started read thread.");

	// continuously read data from serial port
	while (reading_status_) {
		try {
			// read data
			len = serial_port_->read(buffer, MAX_NOUT_SIZE);
		} catch (std::exception &e) {
	        std::stringstream output;
	        output << "Error reading from serial port: " << e.what();
	        log_error_(output.str());
	        //return;
    	}
		// timestamp the read
		if (time_handler_) 
			read_timestamp_ = time_handler_();
		else 
			read_timestamp_ = 0;

		//std::cout << read_timestamp_ <<  "  bytes: " << len << std::endl;
		// add data to the buffer to be parsed
		BufferIncomingData(buffer, len);
	}
	

}

void Novatel::BufferIncomingData(unsigned char *message, unsigned int length)
{
	BINARY_LOG_TYPE message_id;
	// add incoming data to buffer
	for (unsigned int ii=0; ii<length; ii++) {
		// make sure bufIndex is not larger than buffer
		if (buffer_index_ >= MAX_NOUT_SIZE) {
			buffer_index_=0;
            log_warning_("Overflowed receive buffer. Buffer cleared.");
		}

		if (buffer_index_ == 0) {	// looking for beginning of message
			if (message[ii] == 0xAA) {	// beginning of msg found - add to buffer
				data_buffer_[buffer_index_++] = message[ii];
				bytes_remaining_ = 0;
			} else if (message[ii] == '<') {
				// received beginning of acknowledgement
				reading_acknowledgement_ = true;
				buffer_index_ = 1;
			} else {
        //log_debug_("BufferIncomingData::Received unknown data.");
			}
		} else if (buffer_index_ == 1) {	// verify 2nd character of header
			if (message[ii] == 0x44) {	// 2nd byte ok - add to buffer
				data_buffer_[buffer_index_++] = message[ii];
			} else if ( (message[ii] == 'O') && reading_acknowledgement_ ) {
				// 2nd byte of acknowledgement
				buffer_index_ = 2;
			} else {
				// start looking for new message again
				buffer_index_ = 0;
				bytes_remaining_=0;
				reading_acknowledgement_=false;
			} // end if (msg[i]==0x44)
		} else if (buffer_index_ == 2) {	// verify 3rd character of header
			if (message[ii] == 0x12) {	// 2nd byte ok - add to buffer
				data_buffer_[buffer_index_++] = message[ii];
			} else if ( (message[ii] == 'K') && (reading_acknowledgement_) ) {
				// final byte of acknowledgement received
				buffer_index_ = 0;
				reading_acknowledgement_ = false;
				boost::lock_guard<boost::mutex> lock(ack_mutex_);
				ack_received_ = true;
				ack_condition_.notify_all();

				// ACK received
				handle_acknowledgement_();
			} else {
				// start looking for new message again
				buffer_index_ = 0;
				bytes_remaining_ = 0;
				reading_acknowledgement_ = false;
			} // end if (msg[i]==0x12)
		} else if (buffer_index_ == 3) {	// number of bytes in header - not including sync
			data_buffer_[buffer_index_++] = message[ii];
			// length of header is in byte 4
			header_length_ = message[ii];
		} else if (buffer_index_ == 5) { // get message id
			data_buffer_[buffer_index_++] = message[ii];
			bytes_remaining_--;
			message_id = BINARY_LOG_TYPE( ((data_buffer_[buffer_index_-1]) << 8) + data_buffer_[buffer_index_-2] );
		// } else if (buffer_index_ == 8) {	// set number of bytes
		// 	data_buffer_[buffer_index_++] = message[ii];
		// 	// length of message is in byte 8
		// 	// bytes remaining = remainder of header  + 4 byte checksum + length of body
		// 	// TODO: added a -2 to make things work right, figure out why i need this
		// 	bytes_remaining_ = message[ii] + 4 + (header_length_-7) - 2;
		} else if (buffer_index_ == 9) {
			data_buffer_[buffer_index_++] = message[ii];
			bytes_remaining_ = (header_length_ - 10) + 4 + (data_buffer_[9] << 8) + data_buffer_[8];
		} else if (bytes_remaining_ == 1) {	// add last byte and parse
			data_buffer_[buffer_index_++] = message[ii];
			// BINARY_LOG_TYPE message_id = (BINARY_LOG_TYPE) (((data_buffer_[5]) << 8) + data_buffer_[4]);
			// log_info_("Sending to ParseBinary");
			ParseBinary(data_buffer_, buffer_index_, message_id);
			// reset counters
			buffer_index_ = 0;
			bytes_remaining_ = 0;
		} else {	// add data to buffer
			data_buffer_[buffer_index_++] = message[ii];
			bytes_remaining_--;
		}
	}	// end for
}
void 
void Novatel::ParseBinary(unsigned char *message, size_t length, BINARY_LOG_TYPE message_id) {
    //stringstream output;
    //output << "Parsing Log: " << message_id << endl;
    //log_debug_(output.str());
		uint16_t payload_length;
		uint16_t header_length;

		// obtain the received crc
		// for (int)

    switch (message_id) {
        case BESTGPSPOS_LOG_TYPE:
            Position best_gps;

            memcpy(&best_gps, message, sizeof(best_gps));
            if (best_gps_position_callback_)
            	best_gps_position_callback_(best_gps, read_timestamp_);
            break;
        case BESTLEVERARM_LOG_TYPE:
            BestLeverArm best_lever;
            memcpy(&best_lever, message, sizeof(best_lever));
            if (best_lever_arm_callback_)
            	best_lever_arm_callback_(best_lever, read_timestamp_);
            break;
        case BESTPOSB_LOG_TYPE:
            Position best_pos;
            memcpy(&best_pos, message, sizeof(best_pos));
            if (best_position_callback_)
            	best_position_callback_(best_pos, read_timestamp_);
            break;
        case BESTUTMB_LOG_TYPE:
            UtmPosition best_utm;
            memcpy(&best_utm, message, sizeof(best_utm));
            if (best_utm_position_callback_)
           		best_utm_position_callback_(best_utm, read_timestamp_);
            break;
        case BESTVELB_LOG_TYPE:
            Velocity best_vel;
            memcpy(&best_vel, message, sizeof(best_vel));
            if (best_velocity_callback_)
            	best_velocity_callback_(best_vel, read_timestamp_);
            break;
        case BESTXYZB_LOG_TYPE:
            PositionEcef best_xyz;
            memcpy(&best_xyz, message, sizeof(best_xyz));
            if (best_position_ecef_callback_)
            	best_position_ecef_callback_(best_xyz, read_timestamp_);
            break;
        case INSPVA_LOG_TYPE:
            InsPositionVelocityAttitude ins_pva;
            memcpy(&ins_pva, message, sizeof(ins_pva));
            if (ins_position_velocity_attitude_callback_)
            	ins_position_velocity_attitude_callback_(ins_pva, read_timestamp_);
            break;
        case INSPVAS_LOG_TYPE:
            InsPositionVelocityAttitudeShort ins_pva_short;
            memcpy(&ins_pva, message, sizeof(ins_pva_short));
            if (ins_position_velocity_attitude_short_callback_)
            	ins_position_velocity_attitude_short_callback_(ins_pva_short, read_timestamp_);
            break;
        case VEHICLEBODYROTATION_LOG_TYPE:
            VehicleBodyRotation vehicle_body_rotation;
            memcpy(&vehicle_body_rotation, message, sizeof(vehicle_body_rotation));
            if (vehicle_body_rotation_callback_)
            	vehicle_body_rotation_callback_(vehicle_body_rotation, read_timestamp_);
            break;
        case INSSPD_LOG_TYPE:
            InsSpeed ins_speed;
            memcpy(&ins_speed, message, sizeof(ins_speed));
            if (ins_speed_callback_)
            	ins_speed_callback_(ins_speed, read_timestamp_);
            break;
        case RAWIMU_LOG_TYPE:
            RawImu raw_imu;
            memcpy(&raw_imu, message, sizeof(raw_imu));
            if (raw_imu_callback_)
            	raw_imu_callback_(raw_imu, read_timestamp_);
            break;
        case RAWIMUS_LOG_TYPE:
            RawImuShort raw_imu_s;
            memcpy(&raw_imu_s, message, sizeof(raw_imu_s));
            if (raw_imu_short_callback_)
            	raw_imu_short_callback_(raw_imu_s, read_timestamp_);
            break;
        case INSCOV_LOG_TYPE:
            InsCovariance ins_cov;
            memcpy(&ins_cov, message, sizeof(ins_cov));
            if (ins_covariance_callback_)
            	ins_covariance_callback_(ins_cov, read_timestamp_);
            break;
        case INSCOVS_LOG_TYPE:
            InsCovarianceShort ins_cov_s;
            memcpy(&ins_cov_s, message, sizeof(ins_cov_s));
            if (ins_covariance_short_callback_)
            	ins_covariance_short_callback_(ins_cov_s, read_timestamp_);
            break;
        case PSRDOPB_LOG_TYPE:
            Dop psr_dop;
            header_length = (uint16_t) *(message+3);
            payload_length = (((uint16_t) *(message+9)) << 8) + ((uint16_t) *(message+8));

            // Copy header and unrepeated fields
            memcpy(&psr_dop, message, header_length+24);
            //Copy repeated fields
            memcpy(&psr_dop.prn, message+header_length+28, (4*psr_dop.number_of_prns));
            //Copy CRC
            memcpy(&psr_dop.crc, message+header_length+payload_length, 4);

            if (pseudorange_dop_callback_)
            	pseudorange_dop_callback_(psr_dop, read_timestamp_);
            break;
        case RTKDOPB_LOG_TYPE:
            Dop rtk_dop;
            memcpy(&rtk_dop, message, sizeof(rtk_dop));
            if (rtk_dop_callback_)
            	rtk_dop_callback_(rtk_dop, read_timestamp_);
            break;
        case BSLNXYZ_LOG_TYPE:
            BaselineEcef baseline_xyz;
            memcpy(&baseline_xyz, message, sizeof(baseline_xyz));
            if (baseline_ecef_callback_)
            	baseline_ecef_callback_(baseline_xyz, read_timestamp_);
            break;
        case IONUTCB_LOG_TYPE:
            IonosphericModel ion;
            memcpy(&ion, message, sizeof(ion));
            if (ionospheric_model_callback_)
            	ionospheric_model_callback_(ion, read_timestamp_);
            break;
        case RANGEB_LOG_TYPE:
            RangeMeasurements ranges;
            header_length = (uint16_t) *(message+3);
            payload_length = (((uint16_t) *(message+9)) << 8) + ((uint16_t) *(message+8));

            // Copy header and #observations following
            memcpy(&ranges, message, header_length+4);
            //Copy repeated fields
            memcpy(&ranges.range_data, message+header_length+4, (44*ranges.number_of_observations));
            //Copy CRC
            memcpy(&ranges.crc, message+header_length+payload_length, 4);

            if (range_measurements_callback_)
            	range_measurements_callback_(ranges, read_timestamp_);
            break;
        case RANGECMPB_LOG_TYPE: {
	          CompressedRangeMeasurements cmp_ranges;
	        	header_length = (uint16_t) *(message+3);
	        	payload_length = (((uint16_t) *(message+9)) << 8) + ((uint16_t) *(message+8));
	        	// unsigned long crc_of_received = CalculateBlockCRC32(length-4, message);
	        	
	        	// std::stringstream asdf;
	        	// asdf << "------\nheader_length: " << header_length << "\npayload_length: " << payload_length << "\n";
	        	// asdf << "length idx: " << length << "\nsizeof: " << sizeof(cmp_ranges) << "\n";
	        	//asdf << "crc of received: " << crc_of_received << "\n";
	        	// log_info_(asdf.str().c_str()); asdf.str("");

	        	//Copy header and unrepeated message block
	        	memcpy(&cmp_ranges.header,message, header_length);
	        	memcpy(&cmp_ranges.number_of_observations, message+header_length, 4);
	        	// Copy Repeated portion of message block)
                memcpy(&cmp_ranges.range_data, message+header_length+4, (24*cmp_ranges.number_of_observations));
	        	// Copy the CRC
	        	memcpy(&cmp_ranges.crc, message+header_length+payload_length, 4);
	          

	        	// asdf << "sizeof after memcpy : " << sizeof(cmp_ranges) << "\n";
	        	// asdf << "crc after shoving: " ;
						// log_info_(asdf.str().c_str()); asdf.str("");
						// printHex((char*)cmp_ranges.crc,4);
	        	// asdf << "\nMessage from BufferIncomingData\n";
	        	// log_info_(asdf.str().c_str()); asdf.str("");
	        	// printHex((char*)message,length);
asdfadf
	        	
	        	//printHex((char*)cmp_ranges.range_data[0],sizeof(24*((int32_t)*(message+header_length))));

	            // memcpy(&cmp_ranges, message, length);
	            if (compressed_range_measurements_callback_)
		            	compressed_range_measurements_callback_(cmp_ranges, read_timestamp_);
	            break;
          }
        case GPSEPHEMB_LOG_TYPE: {
            GpsEphemeris ephemeris;
            if (length>sizeof(ephemeris)) {
            	std::stringstream ss;
            	ss << "Novatel Driver: GpsEphemeris mismatch\n";
            	ss << "\tlength = " << length << "\n";
	            ss << "\tsizeof msg = " << sizeof(ephemeris);
	            log_warning_(ss.str().c_str());
	          } else {
	            memcpy(&ephemeris, message, sizeof(ephemeris));
	            if (gps_ephemeris_callback_)
	            	gps_ephemeris_callback_(ephemeris, read_timestamp_);
	          }
            break;
        }
        case RAWEPHEMB_LOG_TYPE:
            RawEphemeris raw_ephemeris;
            memcpy(&raw_ephemeris, message, sizeof(raw_ephemeris));
            if (raw_ephemeris_callback_)
                raw_ephemeris_callback_(raw_ephemeris, read_timestamp_);
            break;
        case SATXYZB_LOG_TYPE:
            SatellitePositions sat_pos;
            header_length = (uint16_t) *(message+3);
            payload_length = (((uint16_t) *(message+9)) << 8) + ((uint16_t) *(message+8));

            // Copy header and unrepeated part of message
            memcpy(&sat_pos, message, header_length+12);
            //Copy repeated fields
            memcpy(&sat_pos.data, message+header_length+12, (68*sat_pos.number_of_satellites));
            //Copy CRC
            memcpy(&ranges.crc, message+header_length+payload_length, 4);

            if (satellite_positions_callback_)
            	satellite_positions_callback_(sat_pos, read_timestamp_);
            break;
        case SATVISB_LOG_TYPE:
            SatelliteVisibility sat_vis;
            header_length = (uint16_t) *(message+3);
            payload_length = (((uint16_t) *(message+9)) << 8) + ((uint16_t) *(message+8));

            // Copy header and unrepeated part of message
            memcpy(&sat_pos, message, header_length+12);
            //Copy repeated fields
            memcpy(&sat_vis.data, message+header_length+12, (40*sat_vis.number_of_satellites));
            //Copy CRC
            memcpy(&ranges.crc, message+header_length+payload_length, 4);

            if(satellite_visibility_callback_)
                satellite_visibility_callback_(sat_vis, read_timestamp_);
            break;
        case TIMEB_LOG_TYPE:
            TimeOffset time_offset;
            memcpy(&time_offset, message, sizeof(time_offset));
            if (time_offset_callback_)
            	time_offset_callback_(time_offset, read_timestamp_);
            break;
        case TRACKSTATB_LOG_TYPE:
            TrackStatus tracking_status;
            header_length = (uint16_t) *(message+3);
            payload_length = (((uint16_t) *(message+9)) << 8) + ((uint16_t) *(message+8));

            // Copy header and unrepeated part of message
            memcpy(&tracking_status, message, header_length+16);
            //Copy repeated fields
            memcpy(&tracking_status.data, message+header_length+16, (40*tracking_status.number_of_channels));
            //Copy CRC
            memcpy(&tracking_status.crc, message+header_length+payload_length, 4);

            if(tracking_status_callback_)
                tracking_status_callback_(tracking_status, read_timestamp_);
            break;
        case RXHWLEVELSB_LOG_TYPE:
            ReceiverHardwareStatus hw_levels;
            memcpy(&hw_levels, message, sizeof(hw_levels));
            if (receiver_hardware_status_callback_)
            	receiver_hardware_status_callback_(hw_levels, read_timestamp_);
            break;
        case PSRPOSB_LOG_TYPE:
            Position psr_pos;
            memcpy(&psr_pos, message, sizeof(psr_pos));
            if (best_pseudorange_position_callback_)
            	best_pseudorange_position_callback_(psr_pos, read_timestamp_);
            break;
        case RTKPOSB_LOG_TYPE:
            Position rtk_pos;
            memcpy(&rtk_pos, message, sizeof(rtk_pos));
            if (rtk_position_callback_)
            	rtk_position_callback_(rtk_pos, read_timestamp_);
            break;
        default:
            break;
    }
}

// this functions matches the conversion done by the Novatel receivers
bool Novatel::ConvertLLaUTM(double Lat, double Long, double *northing, double *easting, int *zone, bool *north)
{
     const double a  = 6378137.0;
     const double ee = 0.00669437999;
     const double k0 = 0.9996;
     const double e2 = ee / (1-ee);

     double LongTemp = (Long+180)-int((Long+180)/360)*360-180; // -180.00 .. 179.9;
     double LatRad  = GRAD_A_RAD(Lat);
     double LongRad = GRAD_A_RAD(LongTemp);
     double LongOriginRad;

     double N, T, C, A, M;
     
     //Make sure the longitude is between -180.00 .. 179.9
     *zone = int((LongTemp + 180)/6.0) + 1;
     if (Lat >= 56.0 && Lat < 64.0 && LongTemp >= 3.0 && LongTemp < 12.0)
          *zone = 32;

     // Special zones for Svalbard
     if (Lat >= 72.0 && Lat < 84.0) {
          if (LongTemp>=0.0  && LongTemp<9.0)
               *zone = 31;
          else if (LongTemp>=9.0 && LongTemp<21.0)
               *zone = 33;
          else if (LongTemp>=21.0 && LongTemp<33.0)
               *zone = 35;
          else if (LongTemp>=33.0 && LongTemp<42.0)
               *zone = 37;
     }
     LongOriginRad = GRAD_A_RAD((*zone-1)*6 - 180 + 3);

     N = a/sqrt(1-ee*sin(LatRad)*sin(LatRad));
     T = tan(LatRad)*tan(LatRad);
     C = e2*cos(LatRad)*cos(LatRad);
     A = cos(LatRad)*(LongRad-LongOriginRad);
     M = a*((1 - ee/4 - 3*ee*ee/64 - 5*ee*ee*ee/256)*LatRad
                - (3*ee/8     + 3*ee*ee/32 + 45*ee*ee*ee/1024)*sin(2*LatRad)
                + (15*ee*ee/256 + 45*ee*ee*ee/1024)*sin(4*LatRad)
                - (35*ee*ee*ee/3072)*sin(6*LatRad));
     
     *easting = (double)(k0*N*(A+(1-T+C)*A*A*A/6
                         + (5-18*T+T*T+72*C-58*e2)*A*A*A*A*A/120) + 500000.0);
     *northing = (double)(k0*(M+N*tan(LatRad)*(A*A/2+(5-T+9*C+4*C*C)*A*A*A*A/24
                     + (61-58*T+T*T+600*C-330*e2)*A*A*A*A*A*A/720)));

     if (Lat < 0) {
          *northing += 10000000; //10000000 meter offset for southern hemisphere
          *north = false;
     } else
          *north = true;

     return true;
}


