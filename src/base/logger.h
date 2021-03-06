#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include <vector>
#include <time.h>
#include "str.h"
#include "loki/Singleton.h"

using namespace std;
using namespace ps;
using namespace Loki;


namespace ps {
namespace utils {

class EventLogger;

#define PS_LOG_WRITE_EVENTTYPE 1
#define PS_LOG_WRITE_TIMESTAMP 2
#define PS_LOG_WRITE_SOURCE	   4
#define PS_LOG_WRITE_TO_SCREEN 8
#define PS_LOG_WRITE_TO_MODAL  16

#define PS_LOG_LINE_SIZE	2048
//16 KiloBytes for the memory log
#define PS_LOG_BUFFER_SIZE  8*PS_LOG_LINE_SIZE

//define vargs funcs for logging
#define vloginfo(desc, ...) ps::utils::psLog(ps::utils::EventLogger::etInfo, __FILE__, __LINE__, desc, ##__VA_ARGS__)
#define vlogwarn(desc, ...) ps::utils::psLog(ps::utils::EventLogger::etWarning, __FILE__, __LINE__, desc, ##__VA_ARGS__)
#define vlogerror(desc, ...) ps::utils::psLog(ps::utils::EventLogger::etError, __FILE__, __LINE__, desc, ##__VA_ARGS__)


//Display Error Message
typedef void (*FOnDisplay)(const char* message);


/*!
* Event Logger class for writing major application events to disk. 
*/
class EventLogger {
public:
    EventLogger();

    /*!
        * Constructor for setting the log file path on disk and setting up the flags for
        * controlling how the log is written.
        * @param lpFilePath the string of the file path
        * @param flags that indicate how each log entry has to be written
        */
    EventLogger(const char* lpFilePath, int flags = 0);
    ~EventLogger();

    enum EVENTTYPE {etProfile, etInfo, etWarning, etError};


    //Internal Class for holding an instance of an event
    struct Event{
        EventLogger::EVENTTYPE etype;
        AnsiStr strDesc;
        AnsiStr strSource;
        int value;
    };

    /*!
        * Adds an event to the event log system
        * @param e reference to the event variable
        */
    void add(const Event& e);

    /*!
        * Adds an entry to the log system
        * @param lpStrDesc Description for this event
        * @param t type of this event
        * @param lpSource the source of the event (Can be a File, File + Function Name)
        * @param value can be line number or the error code
        */
    void add(const char* lpStrDesc,
             EVENTTYPE t = etInfo,
             const char* lpStrSource = NULL,
             int value = 0);

    /*!
        * @param flags to control the way log entries are being written to disk
        */
    void setWriteFlags(int flags);

    //Set output filepath
    void setOutFilePath(const char* lpStrFilePath);

    //Set error display callback
    void setDisplayCallBack(FOnDisplay cb) {m_fOnDisplay = cb;}


    //Flush the content of string buffer to disk
    bool flush();
    
    AnsiStr rootPath() const {return m_strRootPath;}
    AnsiStr shortenPathBasedOnRoot(const AnsiStr& strPath) const;
private:

    void display(const char* chrMessage) const;

    //Private Variables
private:
    //On Display Error
    FOnDisplay m_fOnDisplay;

    //Flags to control serialization
    bool m_bWriteEventTypes;
    bool m_bWriteTimeStamps;
    bool m_bWriteSourceInfo;
    bool m_bWriteToScreen;
    bool m_bWriteToModal;

    U32 m_szBufferSize;
    AnsiStr m_strFP;
    AnsiStr m_strRootPath;
    std::vector<AnsiStr> m_lstLog;
};

typedef SingletonHolder<EventLogger, CreateUsingNew, PhoenixSingleton> TheEventLogger;

void psLog(const char* lpDesc, ...);

/*!
* Adds a new entry to log using printf style formatting. 
*/
void psLog(EventLogger::EVENTTYPE etype, const char* lpSource, int line, const char* lpDesc, ...);

}
}

#endif
