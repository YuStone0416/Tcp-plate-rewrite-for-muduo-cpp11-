#include "Timestamp.h"

#include <time.h>
Timestamp::Timestamp():microSecondsDinceEpoch_(0){}
Timestamp::Timestamp(int64_t microSecondsDinceEpoch)    
    :microSecondsDinceEpoch_(microSecondsDinceEpoch)
    {}
Timestamp Timestamp::now()
{
    return Timestamp(time(NULL));
}
std::string Timestamp::toString() const
{
    char buf[128]={0};
    tm *tm_time=localtime(&microSecondsDinceEpoch_);
    //%02d按两位数输出整数，不足两位前面补 0。
    snprintf(buf,128,"%4d/%02d/%02d %02d:%02d:%02d",
        tm_time->tm_year+1900,
        tm_time->tm_mon+1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec);
    return buf;
}
// #include <iostream>
// int main()
// {
//     std::cout<<Timestamp::now().toString()<<std::endl;
//     return 0;
// }