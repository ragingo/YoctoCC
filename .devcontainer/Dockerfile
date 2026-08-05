FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    wget \
    curl \
    git \
    make \
    cmake \
    qemu-user \
    gcc-16 \
    g++-16 \
    gcc-16-x86-64-linux-gnu \
    g++-16-x86-64-linux-gnu \
    libc6-dev-amd64-cross \
    lsb-release \
    software-properties-common \
    gnupg2 \
    gdb \
    bubblewrap \
    socat \
    && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-16 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-16 100

RUN update-alternatives --install /usr/bin/x86_64-linux-gnu-gcc x86_64-linux-gnu-gcc /usr/bin/x86_64-linux-gnu-gcc-16 100 \
    && update-alternatives --install /usr/bin/x86_64-linux-gnu-g++ x86_64-linux-gnu-g++ /usr/bin/x86_64-linux-gnu-g++-16 100

RUN wget https://apt.llvm.org/llvm.sh \
    && chmod +x llvm.sh \
    && ./llvm.sh 22 \
    && rm llvm.sh

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100

RUN rm -f /etc/profile.d/debuginfod.sh

ENV QEMU_LD_PREFIX=/usr/x86_64-linux-gnu

CMD ["/bin/bash"]

