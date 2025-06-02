#pragma once


class copyable
{
    protected: 
        copyable(const copyable&) = default;
        copyable& operator=(const copyable&) = default;

};