/*
 * Copyright (C) 2013 Aleksandar Dezelin <dezelin@gmail.com>.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "mmu.hh"
#include "drivers/virtio.hh"
#include "drivers/virtio-rng.hh"
#include "drivers/pci-device.hh"

#include <osv/debug.h>
#include <osv/device.h>
#include <osv/uio.h>

#include <string>
#include <vector>

namespace virtio {

    #define virtio_rng_tag "virtio-rng"
    #define virtio_rng_d(...)   tprintf_d(virtio_rng_tag, __VA_ARGS__)
    #define virtio_rng_i(...)   tprintf_i(virtio_rng_tag, __VA_ARGS__)
    #define virtio_rng_w(...)   tprintf_w(virtio_rng_tag, __VA_ARGS__)
    #define virtio_rng_e(...)   tprintf_e(virtio_rng_tag, __VA_ARGS__)

    const std::string kDriverName = virtio_rng_tag;
    const std::string kRandomDeviceName = "random";
    const int kRequestQueue = 0;

    int virtio_rng::_instance = 0;

    struct virtio_rng_priv {
        virtio_rng* drv;
    };

    static int virtio_rng_read(struct device *dev, struct uio *uio, int ioflags) 
    {
        struct virtio_rng_priv *prv = 
            reinterpret_cast<struct virtio_rng_priv*>(dev->private_data);
        return prv->drv->make_virtio_request(uio);
    }

    static struct devops virtio_rng_devops = {
        .open   = no_open,
        .close  = no_close,
        .read   = virtio_rng_read,
        .write  = no_write,
        .ioctl  = no_ioctl,
        .devctl = no_devctl,
    };

    struct driver virtio_rng_driver = {
        .name   = "virtio_rng",
        .devops = &virtio_rng_devops,
        .devsz  = sizeof(struct virtio_rng_priv)
    };

    virtio_rng::virtio_rng(pci::device& pci_dev) 
        : virtio_driver(pci_dev)
        , _driver_name(kDriverName)
    {
        virtio_i("VIRTIO RNG INSTANCE");
        _id = _instance++;

        setup_features();

        add_dev_status(VIRTIO_CONFIG_S_DRIVER_OK);

        std::string dev_name(kRandomDeviceName);
        if (_id > 0)
            dev_name += std::to_string(_id - 1);

        struct device *dev = device_create(&virtio_rng_driver, 
            dev_name.c_str(), D_CHR);
        struct virtio_rng_priv *prv = 
            reinterpret_cast<struct virtio_rng_priv*>(dev->private_data);
        prv->drv = this;
    }

    virtio_rng::~virtio_rng() 
    {
    }

    int virtio_rng::make_virtio_request(struct uio* uiop) 
    {
        WITH_LOCK(_lock) {
            if (!uiop)
                return EIO;

            vring* queue = get_virt_queue(kRequestQueue);
            if (!queue) {
                virtio_rng_e("Invalid virtio queue.");
                return EIO;
            }

            size_t size = uiop->uio_resid;
            if (size < 1) {
                virtio_rng_e("Invalid read size.");
                return EIO;
            }

            std::vector<u8> req_buf(size);
            queue->_sg_vec.clear();
            queue->_sg_vec.push_back(vring::sg_node(mmu::virt_to_phys(req_buf.data()), 
                size, vring_desc::VRING_DESC_F_WRITE));

            // Assume there's always room for one buffer
            if (!queue->add_buf(req_buf.data())) {
                virtio_rng_e("No available vring buffers.");
                return EIO;
            }

            virtio_rng_d("Kicking.");
            queue->kick();

            virtio_rng_d("Waiting.");
            virtio_driver::wait_for_queue(queue, &vring::used_ring_not_empty);

            virtio_rng_d("Clearing.");

            queue->get_buf_finalize();
            queue->get_buf_gc();

            int error;
            if ((error = uiomove(req_buf.data(), size, uiop)) != 0) {
                virtio_rng_e("Read failed.");
                return error;                
            }
        }

        return 0;
    }

    u32 virtio_rng::get_driver_features()
    {
        return virtio_driver::get_driver_features();
    }


    hw_driver* virtio_rng::probe(hw_device* dev)
    {
        if (auto pci_dev = dynamic_cast<pci::device*>(dev)) {
            if (pci_dev->get_id() == hw_device_id(VIRTIO_VENDOR_ID, VIRTIO_RNG_DEVICE_ID))
                return new virtio_rng(*pci_dev);
        }

        return nullptr;
    }

} // namespace virtio
