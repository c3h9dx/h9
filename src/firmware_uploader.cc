/*
 * H9 project
 *
 * Created by crowx on 2023-10-19.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#include "firmware_uploader.h"

FirmwareUploader::FirmwareUploader(std::uint8_t* firmware, std::size_t fw_size, std::uint16_t dst):
    node_id(dst),
    recv_frame_count(0),
    sent_frame_count(0),
    firmware(firmware),
    fw_size(fw_size) {
}

void FirmwareUploader::upload(BusProxy* bus) {
    size_t fw_idx = 0;
    uint16_t page = 0;
    // std::cout << "Waiting for node...\n";

    while (true) {
        ExtH9Frame recv_frame = bus->get_frame();

        if (recv_frame.source_id() != node_id) {
            continue;
        }

        ++recv_frame_count;

        if (recv_frame.type() == H9frame::Type::BOOTLOADER_TURNED_ON || recv_frame.type() == H9frame::Type::PAGE_WRITED) {
            if (recv_frame.type() == H9frame::Type::PAGE_WRITED) {
                constexpr int PBWIDTH = 60;
                constexpr char* PBSTR = (char*)"============================================================";

                unsigned int val = fw_idx * 100 / fw_size;
                unsigned int lpad = fw_idx * PBWIDTH / fw_size;
                unsigned int rpad = PBWIDTH - lpad;
                printf("%3u%% [%.*s>%*s]\r", val, lpad, PBSTR, rpad, "");
                fflush(stdout);

                if (fw_idx >= fw_size) {
                    ExtH9Frame frame;
                    frame.type(H9frame::Type::QUIT_BOOTLOADER);
                    frame.destination_id(node_id);
                    frame.dlc(0);

                    bus->put_frame(frame);
                    ++sent_frame_count;

                    printf("\nDone.\n");
                    return;
                }
                page++;
            }
            else if (recv_frame.type() == H9frame::Type::BOOTLOADER_TURNED_ON && recv_frame.dlc() == 5) {
                std::uint8_t bootloader_version_major = recv_frame.data()[0];
                std::uint8_t bootloader_version_minor = recv_frame.data()[1];
                std::uint8_t node_cpu = recv_frame.data()[2];
                std::uint16_t node_type = recv_frame.data()[3];
                node_type <<= 8;
                node_type += recv_frame.data()[4];
                const char* mcu = mcu_map[node_cpu < (sizeof(mcu_map) / sizeof(char*)) ? node_cpu : 0];

                printf("Bootloader version: %hhu.%hhu\n", bootloader_version_major, bootloader_version_minor);
                printf("Target node id: %hu\n", recv_frame.source_id());
                printf("Target node MCU: %s (%hhu)\n", mcu, node_cpu);
                printf("Target node type: %hu\n", node_type);
                printf("Firmware size: %luB\n", fw_size);
            }

            ExtH9Frame frame;
            frame.type(H9frame::Type::PAGE_START);
            frame.destination_id(node_id);
            frame.dlc(2);

            frame.data({(uint8_t)((page >> 8) & 0xff), (uint8_t)((page)&0xff)});

            bus->put_frame(frame);
            ++sent_frame_count;
        }
        else if (recv_frame.type() == H9frame::Type::PAGE_FILL_NEXT) {
            ExtH9Frame frame;
            frame.type(H9frame::Type::PAGE_FILL);
            frame.destination_id(node_id);
            frame.dlc(8);

            frame.data({&firmware[fw_idx], &firmware[fw_idx + 8]});
            fw_idx += 8;

            bus->put_frame(frame);
            ++sent_frame_count;
        }
    }
}