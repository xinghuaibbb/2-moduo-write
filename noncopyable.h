#pragma once

class noncopyable
{
    /*
    noncopyable被继承以后, 派生类对象可以正常的构造和析构,
    但派生类对象不能被拷贝和赋值
    */

public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;

};