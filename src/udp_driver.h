/*
* H9 project
*
* Created by SQ8KFH on 2024-04-19.
*
* Copyright (C) 2024 Kamil Palkowski. All rights reserved.
*/

#pragma once

#include <netdb.h>

#include "bus_driver.h"

class UDPDriver: public BusDriver {
 private:
   const std::string local_port;
   const std::string remote_addr;
   const std::string remote_port;

   addrinfo *remote;

 public:
   explicit UDPDriver(const std::string& name, std::string local_port, std::string remote_addr, std::string remote_port);
   ~UDPDriver();
   int open();

 private:
   int recv_data(H9frame* frame);
   int send_data(std::shared_ptr<BusFrame> busframe);
};
