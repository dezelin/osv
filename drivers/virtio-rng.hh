/*
 * Copyright (C) 2013 Aleksandar Dezelin <dezelin@gmail.com>.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef VIRTIO_RNG_DRIVER_H
#define VIRTIO_RNG_DRIVER_H

#include "drivers/virtio.hh"
#include "drivers/pci-device.hh"

#include <osv/uio.h>

namespace virtio {

    class virtio_rng : public virtio_driver {
    public:

        enum {
            VIRTIO_RNG_DEVICE_ID = 0x1005
        };

        explicit virtio_rng(pci::device& pci_dev);
        virtual ~virtio_rng();

        virtual const std::string get_name() { return _driver_name; }
        virtual u32 get_driver_features();

        int make_virtio_request(struct uio* uiop);

        static hw_driver* probe(hw_device* dev);

    private:
        std::string _driver_name;

        //maintains the virtio instance number for multiple drives
        static int _instance;
        int _id;

        // This mutex protects parallel make_request invocations
        mutex _lock;
    };

} // namespace virtio

#endif // VIRTIO_RNG_DRIVER_H
