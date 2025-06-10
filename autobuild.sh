#!/bin/bash

set -e  # 如果脚本中的任何命令失败，脚本将立即退出

if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build  # 如果构建目录不存在，则创建它
fi

rm -rf `pwd`/build/* # 清理构建目录

cd `pwd`/build &&
    cmake .. &&
    make

cd .. # 回到项目根目录

# 把头文件拷贝到 /usr/include/hzhmuduo so库拷贝到 /usr/lib   PATH
if [ ! -d /usr/include/hzhmuduo ]; then
    mkdir -p /usr/include/hzhmuduo
fi

for header in `ls *.h`
do
    cp $header /usr/include/hzhmuduo/
done

cp `pwd`/lib/libhzhmuduo.so /usr/lib/

ldconfig  # 更新动态链接器缓存




