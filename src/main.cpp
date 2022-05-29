#include <iostream>
#include <string>
#include <thread>

#include "main.h"
#include "monitorEvents.h"


bool logEnabled = false;

using namespace std;

void log(string to_log)
{
    if(logEnabled == true)
    {
        std::cout << to_log << std::endl;
    }
}

int main()
{
    const char* tmp = std:: getenv("DEBUG");
    std::string envVar;

    if(tmp != NULL)
    {
        envVar = tmp;
        if(envVar == "true")
        {
            logEnabled = true;
            log("Debug mode is activated");
        }
    }
    std::thread monitor_dev(startMonitoringDev);
    // https://stackoverflow.com/questions/7381757/c-terminate-called-without-an-active-exception
    monitor_dev.join();
    
    return 0;
}