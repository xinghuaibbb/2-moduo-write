#pragma once

#include <iostream>


// 时间类
class Timestamp 
{
public:
    Timestamp();

    explicit Timestamp(int64_t microSecondsSinceEpoch);

    ~Timestamp()=default;

    static Timestamp now();

    std::string toString() const;


private:
    int64_t microSecondsSinceEpoch_; //  

};