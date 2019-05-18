# Overview
chain-net is a cross-platform CXX library for network IO，Provides built-in IO buffer and completion event callback interface

# Feature
### Unsafe-Code
hmmmm.. lueluelue

### Lock-Free
*Go to [Documentation](https://github.com/Xarvie/chain-net#documentation) to view details*

### Zero-copy (WIP)
In most cases, it is Zero-Copy. If the Socket send buffer is full, it will copy the data into the library's built-in IO buffer (This should be avoided during development.)

### Follow the Unix design philosophy
>*"small and beautiful"*

### Crose-Platform
High performance on Unix-Like-System
- kqueue Based: FreeBSD / OpenBSD / MacOSX
- Epoll Based: CentOS / Fedora / Ubuntu / Debian / OpenSUSE
- IOCP Based(WIP): Solaris / Windows
- Select Based for Debug: Solaris / Windows / Unix

# Advanced features
### Customizable memory allocator

*Go to [Documentation](https://github.com/Xarvie/chain-net#documentation) to view details*

# Licensing

chain-net is licensed under the MIT license. 

# Documentation

None

# TO-DO list

- Unit test case
- Ipv6
- Connector

# Build environment
``` bash
git clone https://github.com/Xarvie/chain-net.git
cd chain-net
mkdir build && cd build
cmake ..
cmake --build .
```
