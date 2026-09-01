#include "utility.hpp"



// function to generate a random 32-int number
ID generate_random_uint32()
{
    std::random_device rd;  // seed source
    std::mt19937 gen(rd());  // 32-bit Mersenne Twister PRNG
    std::uniform_int_distribution<ID> dist(0, UINT32_MAX);
    return dist(gen);
}



// function to get current time as an integer (milliseconds since Unix epoch)
Time get_current_time_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// function to convert milliseconds timestamp to a human-readable string : "YYYY-MM-DD HH:MM:SS.mmm"
std::string time_to_string(Time time_ms)
{
    // convert milliseconds to seconds
    std::time_t seconds = time_ms / MS_IN_S;
    int milliseconds = time_ms % MS_IN_S;

    // convert to local time
    std::tm now_tm = *std::localtime(&seconds);

    // format the time in a readable way (YYYY-MM-DD HH:MM:SS.mmm)
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, 
        now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec, milliseconds);
    return std::string(buffer);
}

// get the time in the day : 16h05m23.123s -> 16*60*60*1000 + 5*60*1000 + 23*1000 + 123
ID get_daily_time(Time time_ms)
{
    // convert milliseconds to seconds
    std::time_t seconds = time_ms / MS_IN_S;
    int milliseconds = time_ms % MS_IN_S;

    // convert to local time
    std::tm now_tm = *std::localtime(&seconds);

    // get the time in the day in milliseconds
    return milliseconds + MS_IN_S * (now_tm.tm_sec + S_IN_M * (now_tm.tm_min + M_IN_H * now_tm.tm_hour));
}

// get the date : 2025-03-18 is 2025*12*31 + 3*31 + 18, and to get it we need to divide the result
ID get_date_time(Time time_ms)
{
    // convert milliseconds to seconds
    std::time_t seconds = time_ms / MS_IN_S;

    // convert to local time
    std::tm now_tm = *std::localtime(&seconds);

    // get the date in the year
    return now_tm.tm_mday + D_IN_M * (now_tm.tm_mon + M_IN_Y * now_tm.tm_year);
}

// convert the daily time and date time to a string (YYYY-MM-DD HH:MM:SS.mmm)
std::string two_times_to_string(ID date_time, ID daily_time)
{
    // get the time in the day
    int hours = daily_time / MS_IN_H;
    daily_time %= MS_IN_H;
    int minutes = daily_time / MS_IN_M;
    daily_time %= MS_IN_M;
    int seconds = daily_time / MS_IN_S;
    int milliseconds = daily_time % MS_IN_S;

    // get the date
    int year = date_time / (D_IN_M * M_IN_Y);
    date_time %= (D_IN_M * M_IN_Y);
    int month = date_time / D_IN_M;
    int day = date_time % D_IN_M;

    // format the time in the correct readable way (YYYY-MM-DD HH:MM:SS.mmm)
    // Reserve and format directly
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
    year + 1900, month + 1, day, 
    hours, minutes, seconds, milliseconds);
    return std::string(buffer);
}

// get the date from a string : "YYYY-MM-DD" -> YYYY*12*31 + MM*31 + DD
ID get_date_id_from_string(const std::string& date_str)
{
    int year = std::stoi(date_str.substr(0, 4));   // characters 0-3
    int month = std::stoi(date_str.substr(5, 2));  // characters 5-6
    int day = std::stoi(date_str.substr(8, 2));    // characters 8-9
    return day + D_IN_M * (month + M_IN_Y * year);
}

// get the daily time from a string : "HH:MM:SS.mmm" -> HH*60*60*1000 + MM*60*1000 + SS*1000 + mmm
ID get_daily_id_from_string(const std::string& time_str)
{
    int hour = std::stoi(time_str.substr(0, 2));        // characters 0-1
    int minute = std::stoi(time_str.substr(3, 2));      // characters 3-4
    int second = std::stoi(time_str.substr(6, 2));      // characters 6-7
    int millisecond = std::stoi(time_str.substr(9, 3)); // characters 9-11
    return millisecond + MS_IN_S * (second + S_IN_M * (minute + M_IN_H * hour));
}


// function to check if a specific key is pressed on Mac OS
bool key_pressed(const char& keyboard_touch)
{
    struct termios oldt, newt;
    int oldf;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK); 
    int result = read(STDIN_FILENO, &ch, 1); 
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    return (result > 0 && ch == keyboard_touch);
}


// global key and IV for AES encryption
unsigned char key[AES_KEY_SIZE], iv[AES_IV_SIZE];

// encrypt function
std::string encrypt_AES(const std::string& plaintext, unsigned char* key, unsigned char* iv)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv);

    std::string ciphertext(plaintext.size() + AES_IV_SIZE, '\0');
    int len;
    EVP_EncryptUpdate(ctx, (unsigned char*)&ciphertext[0], &len, (unsigned char*)plaintext.c_str(), plaintext.size());
    int final_len;
    EVP_EncryptFinal_ex(ctx, (unsigned char*)&ciphertext[len], &final_len);
    EVP_CIPHER_CTX_free(ctx);
    
    ciphertext.resize(len + final_len);
    return ciphertext;
}

// decrypt function
std::string decrypt_AES(const std::string& ciphertext, unsigned char* key, unsigned char* iv)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv);

    std::string plaintext(ciphertext.size(), '\0');
    int len;
    EVP_DecryptUpdate(ctx, (unsigned char*)&plaintext[0], &len, (unsigned char*)ciphertext.c_str(), ciphertext.size());
    int final_len;
    EVP_DecryptFinal_ex(ctx, (unsigned char*)&plaintext[len], &final_len);
    EVP_CIPHER_CTX_free(ctx);

    plaintext.resize(len + final_len);
    return plaintext;
}

// 
void send_full_string(int sock, const std::string &data, std::mutex &send_mtx)
{
    std::lock_guard<std::mutex> lock(send_mtx);
    uint64_t msg_size = data.size();
    uint64_t net_size = htobe64(msg_size);

    ssize_t sent = send(sock, &net_size, sizeof(net_size), 0);
    if (sent != sizeof(net_size)){
        throw std::runtime_error("Failed to send message size header");
    }

    size_t total = 0;
    while (total < data.size()){
        ssize_t bytes = send(sock, data.data() + total, data.size() - total, 0);
        if (bytes <= 0){
            throw std::runtime_error("Socket send error");
        }
        total += bytes;
    }
}

// send safely a message through a socket
std::string recv_full_string(int sock, std::string &leftover, std::mutex &recv_mtx, int timeout_sec = 10)
{   
    std::lock_guard<std::mutex> lock(recv_mtx);
    uint64_t net_size;
    size_t need = sizeof(net_size);

    // If leftover has enough for size header, use it
    if (leftover.size() < need){
        char tmp[BUFFER_SIZE];
        while (leftover.size() < need){
            ssize_t r = recv(sock, tmp, sizeof(tmp), 0);
            if (r <= 0){
                throw std::runtime_error("Failed to read size header");
            }
            leftover.append(tmp, r);
        }
    }

    std::memcpy(&net_size, leftover.data(), sizeof(net_size));
    leftover.erase(0, sizeof(net_size));

    uint64_t msg_size = be64toh(net_size);
    std::string data;
    data.reserve(msg_size);

    // consume leftover first
    if (!leftover.empty()){
        size_t use = msg_size < leftover.size() ? msg_size : leftover.size();
        data.append(leftover.substr(0, use));
        leftover.erase(0, use);
    }

    // then read remainder
    while (data.size() < msg_size){
        char tmp[BUFFER_SIZE];
        ssize_t r = recv(sock, tmp, sizeof(tmp), 0);
        if (r <= 0){
            throw std::runtime_error("Connection lost during receive");
        }
        size_t needed = msg_size - data.size();
        if ((size_t)r > needed){
            data.append(tmp, needed);
            leftover.assign(tmp + needed, r - needed);
        } 
        else {
            data.append(tmp, r);
        }
    }
    return data;
}
