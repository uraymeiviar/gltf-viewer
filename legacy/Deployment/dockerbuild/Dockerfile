FROM ubuntu:18.04
# install vulkan-sdk
RUN apt update; apt install -y wget gnupg git git-lfs; 
RUN wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | apt-key add -
RUN wget -qO /etc/apt/sources.list.d/lunarg-vulkan-bionic.list https://packages.lunarg.com/vulkan/lunarg-vulkan-bionic.list
RUN apt update; apt install -y vulkan-sdk

#install qt
RUN wget https://download.qt.io/archive/qt/5.14/5.14.2/qt-opensource-linux-x64-5.14.2.run
RUN chmod +x qt-opensource-linux-x64-5.14.2.run
RUN unshare -n ./qt-opensource-linux-x64-5.14.2.run  -platform minimal --script ./qt-installer-noninteractive.qs
ENV LD_LIBRARY_PATH $HOME/Qt/5.14.2/gcc_64/lib
ENV PATH $PATH:$HOME/Qt/5.14.2/gcc_64/bin

#install build dependencies
RUN apt install -y python3 python3-pip
RUN pip3 install wget

#install current cmake version (ubuntu 18.04's is to low)
RUN wget https://github.com/Kitware/CMake/releases/download/v3.18.2/cmake-3.18.2-Linux-x86_64.sh
RUN sh cmake-3.18.2-Linux-x86_64.sh --skip-license --exclude-subdir --prefix=/usr/local

#install opengl development libraries
RUN apt install -y libgl1-mesa-dev