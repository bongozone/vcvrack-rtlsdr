#pragma once

#include <rtl-sdr.h>
#include <pthread.h>
#include <libusb.h>

struct RtlSdr {
};
void RtlSdr_init(struct RtlSdr* radio);
