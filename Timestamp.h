#pragma once

#include <iostream>
#include <string>

//时间类
class Timestamp
{
public:
    Timestamp();
    //explicit让构造函数/转换函数只能“显式调用”，避免编译器偷偷帮你做类型转换，提升代码安全性。主要用于单个参数
    explicit Timestamp(int64_t microSecondsDinceEpoch);
    static Timestamp now();
    std::string toString() const;
private:
    int64_t microSecondsDinceEpoch_;
};