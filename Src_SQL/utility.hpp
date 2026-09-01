//==========================================================================
// File that included all the helper and support functions
//==========================================================================
#ifndef UTILITY_HPP
#define UTILITY_HPP



#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <termios.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>



#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#else
#include <endian.h>
#endif



#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>



/////////////////////////////////////////////////////////////////////////////////////
// other helper functions
/////////////////////////////////////////////////////////////////////////////////////
using ID = int64_t; // type for the id of the different elements in the tables (but also used for the time)
// function to generate a random 32-int number
ID generate_random_uint32();
#define max_number UINT16_MAX // maximum price for an action, time limits
#define safety_percentage 1.10 // percentage of safety margin for the price of an action when consider to know if the client has enough money to buy an action


/////////////////////////////////////////////////////////////////////////////////////
// Defining all the constants link to the time management
/////////////////////////////////////////////////////////////////////////////////////
// since we make delays in milliseconds
#define MS_IN_S 1000
#define S_IN_M 60
#define MS_IN_M 60000
#define M_IN_H 60
#define MS_IN_H 3600000
#define D_IN_M 31
#define M_IN_Y 12
using Time = uint64_t; // type for the time in the game

// function to get current time as an integer (milliseconds since Unix epoch : 1970-01-01 00:00:00 UTC)
Time get_current_time_ms();
// function to convert milliseconds timestamp to a human-readable string : "YYYY-MM-DD HH:MM:SS.mmm"
std::string time_to_string(Time time_ms);

// get the time in the day : 16h05m23.123s -> 16*60*60*1000 + 5*60*1000 + 23*1000 + 123
ID get_daily_time(Time time_ms);
// get the date : 2025-03-18 is 2025*12*31 + 3*31 + 18, and to get it we need to divide the result
ID get_date_time(Time time_ms);
// convert the daily time and date time to a string (YYYY-MM-DD HH:MM:SS.mmm)
std::string two_times_to_string(ID date_time, ID daily_time);
// get the date from a string : "YYYY-MM-DD" -> YYYY*12*31 + MM*31 + DD
ID get_date_id_from_string(const std::string& date_str);
// get the daily time from a string : "HH:MM:SS.mmm" -> HH*60*60*1000 + MM*60*1000 + SS*1000 + mmm
ID get_daily_id_from_string(const std::string& time_str);



/////////////////////////////////////////////////////////////////////////////////////
// Defining all the constants link to the network management
/////////////////////////////////////////////////////////////////////////////////////
//IP Tom : "147.250.233.58"
//IP Thomas : "147.250.227.175"
//IP Local Host : "127.0.0.1"
#define SERVER_IP "127.0.0.1"  // Adresse du serveur (localhost pour les tests locaux)
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024


    
/////////////////////////////////////////////////////////////////////////////////////
// handling key board input without SDL
/////////////////////////////////////////////////////////////////////////////////////
// function to check if a specific key is pressed on Mac OS
bool key_pressed(const char& keyboard_touch);



/////////////////////////////////////////////////////////////////////////////////////
// Password encryption and decryption
/////////////////////////////////////////////////////////////////////////////////////
#define AES_KEY_SIZE 32   // 256-bit key
#define AES_IV_SIZE 16    // Initialization Vector
extern unsigned char key[AES_KEY_SIZE], iv[AES_IV_SIZE]; // global key and IV for AES encryption

// function to handle errors
void handle_errors();

// encrypt function
std::string encrypt_AES(const std::string& plaintext, unsigned char* key, unsigned char* iv);

// decrypt function
std::string decrypt_AES(const std::string& ciphertext, unsigned char* key, unsigned char* iv);



/////////////////////////////////////////////////////////////////////////////////////
// Sending and receiving messages over the network
/////////////////////////////////////////////////////////////////////////////////////
// send safely a message through a socket
void send_full_string(int sock, const std::string &data, std::mutex &send_mtx);

// read safely a message from a socket
std::string recv_full_string(int sock, std::string &leftover, std::mutex &recv_mtx, int timeout_sec);
// leftover is a per-client buffer that stores extra bytes read from the socket beyond the current message, ensuring no data is lost. 
// It allows the client to correctly handle partial or multiple messages in a TCP stream across successive recv() calls.



#endif // UTILITY_HPP