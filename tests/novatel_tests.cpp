#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "novatel/novatel_enums.h"
#include "novatel/novatel_structures.h"
#include <string_utils/string_utils.h>

// OMG this is so nasty...
#define private public
#define protected public

#include "novatel/novatel.h"
using namespace novatel;

TEST(StructureSizeTest, Headers) {
	ASSERT_EQ(HEADER_SIZE, sizeof(Oem4BinaryHeader));
	ASSERT_EQ(SHORT_HEADER_SIZE, sizeof(OEM4ShortBinaryHeader));
}

TEST(StructureSizeTest, IMUMessageStructures) {
	ASSERT_EQ(HEADER_SIZE+92, sizeof(InsPositionVelocityAttitude));
	ASSERT_EQ(SHORT_HEADER_SIZE+92, sizeof(InsPositionVelocityAttitudeShort));
	ASSERT_EQ(HEADER_SIZE+52, sizeof(VehicleBodyRotation));
	ASSERT_EQ(HEADER_SIZE+44, sizeof(InsSpeed));
	ASSERT_EQ(HEADER_SIZE+44, sizeof(RawImu));
	ASSERT_EQ(SHORT_HEADER_SIZE+44, sizeof(RawImuShort));
	ASSERT_EQ(HEADER_SIZE+56, sizeof(BestLeverArm));
	ASSERT_EQ(HEADER_SIZE+232, sizeof(InsCovariance));
	ASSERT_EQ(SHORT_HEADER_SIZE+232, sizeof(InsCovarianceShort));
	ASSERT_EQ(HEADER_SIZE+76, sizeof(BestGpsPosition));
}

TEST(StructureSizeTest, GPSMessageStructures) {
	ASSERT_EQ(HEADER_SIZE+76, sizeof(BestPosition));
	ASSERT_EQ(HEADER_SIZE+84, sizeof(BestUtmPosition));
	ASSERT_EQ(HEADER_SIZE+48, sizeof(BestVelocity));
	ASSERT_EQ(HEADER_SIZE+116, sizeof(PseudorangePositionECEF));
	ASSERT_EQ(HEADER_SIZE+60, sizeof(BaselineECEF));
	ASSERT_EQ(HEADER_SIZE+116, sizeof(BestPositionECEF));
}

//TEST(StructureSizeTest, StatusStructures) {
//	ASSERT_EQ(HEADER_SIZE+112, sizeof(Version));
//}

TEST(DataParsing, Oem4SpanVersion) {
	// load data file and pass through parse methods
	std::ifstream test_datafile;
	test_datafile.open("/home/hododav/Development/sensors/novatel/tests/"
			"test_data/ascii_version_test_data_oem4_span.log");

	if (test_datafile.is_open()) {
		// read data from the file and pass to the novatel parse methods
		std::string file_contents((std::istreambuf_iterator<char>(test_datafile)),
				std::istreambuf_iterator<char>());

		Novatel my_gps;

		std::vector<std::string> packets;

		string_utils::Tokenize(file_contents, packets, "\n");

		// loop through all packets in file and check for version messages
		// stop when the first is found or all packets are read
		for (size_t ii=0; ii<packets.size(); ii++) {
			if (my_gps.ParseVersion(packets[ii]))
				break;
		}

		// check values
		ASSERT_TRUE(my_gps.span_capable_);
		ASSERT_TRUE(my_gps.rtk_capable_);
		ASSERT_FALSE(my_gps.glonass_capable_);
		ASSERT_TRUE(my_gps.l2_capable_);
		ASSERT_TRUE(my_gps.raw_capable_);

	} else {
		// fail the test if the file can't be opened
		std::cout << "Test file could not be opened." << std::endl;
		ASSERT_TRUE(false);
	}
}

//TEST(DataParsing, Oem5Dlv3Version) {
//	// load data file and pass through parse methods
//	std::ifstream test_datafile;
//	test_datafile.open("/home/hododav/Development/sensors/novatel/tests/"
//			"test_data/ascii_version_test_data_oem5_dlv3.log");

//	if (test_datafile.is_open()) {
//		// read data from the file and pass to the novatel parse methods
//		std::string file_contents((std::istreambuf_iterator<char>(test_datafile)),
//				std::istreambuf_iterator<char>());

//		Novatel my_gps;

//		std::vector<std::string> packets;

//		string_utils::Tokenize(file_contents, packets, "\n");

//		// loop through all packets in file and check for version messages
//		// stop when the first is found or all packets are read
//		for (size_t ii=0; ii<packets.size(); ii++) {
//			if (my_gps.ParseVersion(packets[ii]))
//				break;
//		}

//		// check values
//		ASSERT_FALSE(my_gps.span_capable_);
//		ASSERT_TRUE(my_gps.rtk_capable_);
//		ASSERT_FALSE(my_gps.glonass_capable_);
//		ASSERT_TRUE(my_gps.l2_capable_);
//		ASSERT_TRUE(my_gps.raw_capable_);

//	} else {
//		// fail the test if the file can't be opened
//		std::cout << "Test file could not be opened." << std::endl;
//		ASSERT_TRUE(false);
//	}
//}

TEST(DataParsing, Oem5PropakGlonassVersion) {
    // load data file and pass through parse methods
    std::ifstream test_datafile;
    test_datafile.open("/home/hododav/Development/sensors/novatel/tests/"
            "test_data/ascii_version_test_data_oem5_propak3_glonass.log");

    if (test_datafile.is_open()) {
        // read data from the file and pass to the novatel parse methods
        std::string file_contents((std::istreambuf_iterator<char>(test_datafile)),
                std::istreambuf_iterator<char>());

        Novatel my_gps;

        std::vector<std::string> packets;

        string_utils::Tokenize(file_contents, packets, "\n");

        // loop through all packets in file and check for version messages
        // stop when the first is found or all packets are read
        for (size_t ii=0; ii<packets.size(); ii++) {
            if (my_gps.ParseVersion(packets[ii]))
                break;
        }

        // check values
        ASSERT_FALSE(my_gps.span_capable_);
        ASSERT_TRUE(my_gps.rtk_capable_);
        ASSERT_TRUE(my_gps.glonass_capable_);
        ASSERT_TRUE(my_gps.l2_capable_);
        ASSERT_TRUE(my_gps.raw_capable_);

    } else {
        // fail the test if the file can't be opened
        std::cout << "Test file could not be opened." << std::endl;
        ASSERT_TRUE(false);
    }
}

TEST(DataParsing, BinaryDataSet1) {
    // load data file and pass through parse methods
    std::ifstream test_datafile;
    test_datafile.open("/home/hododav/Development/sensors/novatel/tests/"
            "test_data/OneEach.GPS",ios::in|ios::binary);

    if (test_datafile.is_open()) {
        // read data from the file and pass to the novatel parse methods
        Novatel my_gps;
        char *file_data = new char[1000];
        while (!test_datafile.eof())
        {
            test_datafile.read(file_data, sizeof(file_data));
            my_gps.BufferIncomingData((unsigned char*)file_data,test_datafile.gcount());
        }

    } else {
        // fail the test if the file can't be opened
        std::cout << "Test file could not be opened." << std::endl;
        ASSERT_TRUE(false);
    }
}


int main(int argc, char **argv) {
  try {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
  } catch (std::exception &e) {
    std::cerr << "Unhandled Exception: " << e.what() << std::endl;
  }
  return 1;
}