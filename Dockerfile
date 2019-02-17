FROM debian:9.7
MAINTAINER Neil Zhao, i@zzrcxb.me

WORKDIR /root/

RUN apt update && \
    apt install -y wget build-essential cmake llvm clang python3 python3-pip && \
    pip3 install angr termcolor

RUN ["/bin/bash", "-c", \
    "set -o pipefail && \
    wget -qO- https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz | tar xzf - -C /root/ && \
    mv /root/pin-3.7* /root/pin-3.7 && \
    echo PIN_ROOT=/root/pin-3.7 >> /etc/environment"]

COPY . /root/fusor
RUN cd fusor && mkdir build && cd build && cmake .. && make && \
    echo FUSOR_CONFIG=/root/fusor/config.json >> /etc/environment
