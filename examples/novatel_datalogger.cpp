#include <string>
#include <fstream>
#include "novatel/novatel.h"

using namespace novatel;
using namespace std;

std::fstream file_;
//************************************************creat two more of these for gpesephemB and rangecmpb
void BestGpsPositionCallback(Position best_position, double time_stamp) {






}

void GpsEphemerisCallback(GpsEphemeris ephimeris, double time_stamp) {





}

void CompressedRangeMeasurementsCallback(CompressedRangeMeasurements compressed_range, double time_stamp)  {






}


bool OpenFile(std::string name) {
    file.open(name.c_str(),ios::out, ios::binary, ios::app);
    return file.is_open();
}

void WriteToFile(unsigned char *message, size_t length) {
    if(file.is_open())
    {
        file.write(message, length);
    }
}


int main(int argc, char **argv)
{
    if(argc < 3) {
        std::cerr << "Usage: novatel_example <serial port address> <baud rate>" << std::endl;
        return 0;
    }
    std::string port(argv[1]);
    int baudrate=115200;
    istringstream(argv[2]) >> baudrate;

    Novatel my_gps;

    my_gps.set_best_gps_position_callback(
                boost::bind(&BestGpsPositionCallback, this, _1, _2));


    my_gps.set_gps_ephemeris_callback(
                boost::bind(&GpsEphemerisCallback, this, _1, _2));


     my_gps.set_compressed_range_measurements_callback(
                boost::bind(&CompressedRangeMeasurementsCallback, this, _1, _2));




    bool result = my_gps.Connect(port,baudrate);
    if (result) {
        cout << "Successfully connected." << endl;
    }
    else {
        cout << "Failed to connect." << endl;
        return 0;
    }



    while(1);

    my_gps.Disconnect();

    return 0;
}
