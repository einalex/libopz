
#include "OPZ_RtMidi.h"

namespace T3 {

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef PLATFORM_WINDOWS
const int CLOCK_MONOTONIC = 0;
int clock_gettime(int, struct timespec* spec)      //C-file part
{
    __int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime);
    wintime -= 116444736000000000i64;  //1jan1601 to 1jan1970
    spec->tv_sec = wintime / 10000000i64;           //seconds
    spec->tv_nsec = wintime % 10000000i64 * 100;      //nano-seconds
    return 0;
}
#endif

static timespec time_start;
double getTimeSec(const timespec &time_start) {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    timespec temp;
    if ((now.tv_nsec-time_start.tv_nsec)<0) {
        temp.tv_sec = now.tv_sec-time_start.tv_sec-1;
        temp.tv_nsec = 1000000000+now.tv_nsec-time_start.tv_nsec;
    }
    else {
        temp.tv_sec = now.tv_sec-time_start.tv_sec;
        temp.tv_nsec = now.tv_nsec-time_start.tv_nsec;
    }
    return double(temp.tv_sec) + double(temp.tv_nsec/1000000000.);
}

OPZ_RtMidi::OPZ_RtMidi() : 
m_in(NULL), m_out(NULL),
m_last_heartbeat(0.0), m_last_time(0.0) {
}

bool OPZ_RtMidi::connect() {

    m_in = new RtMidiIn();
    unsigned int nPorts = m_in->getPortCount();
    bool in_connected = false;
    for(unsigned int i = 0; i < nPorts; i++) {
        std::string name = m_in->getPortName(i);
        if (name.rfind("OP-Z", 0) == 0) {
            try {
                m_in = new RtMidiIn(RtMidi::Api(0), "opz_dump");
                m_in->openPort(i, name);
                m_in->ignoreTypes(false, false, true);
                m_in->setCallback(process_message, this);
                in_connected = true;
                break;
            } catch(RtMidiError &error) {
                error.printMessage();
            }
        }
    }

    if (in_connected) {
        m_out = new RtMidiOut();
        nPorts = m_out->getPortCount();
        for(unsigned int i = 0; i < nPorts; i++) {
            std::string name = m_out->getPortName(i);
            if (name.rfind("OP-Z", 0) == 0) {
                try {
                    clock_gettime(CLOCK_MONOTONIC, &time_start);

                    m_out = new RtMidiOut(RtMidi::Api(0), "opz_dump");
                    m_out->openPort(i, name);
                    m_out->sendMessage( getInitMsg() );
                    return true;
                }
                catch(RtMidiError &error) {
                    error.printMessage();
                }
            }
        }
    }

    disconnect();
    return false;
}

void OPZ_RtMidi::keepawake(){
    double now = getTimeSec(time_start);; 
    double delta = now - m_last_time;
    m_last_time = now;

    m_last_heartbeat += delta;

    if (m_last_heartbeat > 1.0) {
        m_out->sendMessage( getHeartBeat() );
        m_last_heartbeat = 0.0;
    }

    usleep( 16700 );
}

void OPZ_RtMidi::disconnect() {
    if (m_in) {
        m_in->cancelCallback();
        m_in->closePort();
        delete m_in;
        m_in = NULL;
    }

    if (m_out) {
        m_out->closePort();
        delete m_out;
        m_out = NULL;
    }
}

}
