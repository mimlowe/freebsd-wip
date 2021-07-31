/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Michael Lowe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>

struct vtfs_softc {
	device_t		 vtfs_dev;
	uint64_t		 vtfs_features;
	struct virtqueue	*vtfs_reqvq;
	struct virtqueue	*vtfs_hipriovq;
};

static int	vtfs_modevent(module_t, int, void *);

static int	vtfs_probe(device_t);
static int	vtfs_attach(device_t);
static int	vtfs_detach(device_t);

static int	vtfs_negotiate_features(struct vtfs_softc *);
static int	vtfs_alloc_hiprio_virtqueue(struct vtfs_softc *);
static int	vtfs_alloc_req_virtqueue(struct vtfs_softc *);
#define vtfs_FEATURES	0

static struct virtio_feature_desc vtfs_feature_desc[] = {
	{ 0, NULL }
};

static device_method_t vtfs_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtfs_probe),
	DEVMETHOD(device_attach,	vtfs_attach),
	DEVMETHOD(device_detach,	vtfs_detach),

	DEVMETHOD_END
};

static driver_t vtfs_driver = {
	"vtfs",
	vtfs_methods,
	sizeof(struct vtfs_softc)
};
static devclass_t vtfs_devclass;

VIRTIO_DRIVER_MODULE(virtio_fs, vtfs_driver, vtfs_devclass, vtfs_modevent, 0);
MODULE_VERSION(virtio_fs, 1);
MODULE_DEPEND(virtio_fs, virtio, 1, 1, 1);
// MODULE_DEPEND(virtio_fs, random_device, 1, 1, 1);

VIRTIO_SIMPLE_PNPINFO(virtio_fs, VIRTIO_ID_FS, "VirtIO Filesystem Adapter");

static int
vtfs_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	case MOD_QUIESCE:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
vtfs_probe(device_t dev)
{
	return (VIRTIO_SIMPLE_PROBE(dev, virtio_fs));
}

static int
vtfs_attach(device_t dev)
{
	struct vtfs_softc *sc, *exp;
	int error;

	sc = device_get_softc(dev);
	sc->vtfs_dev = dev;
	virtio_set_feature_desc(dev, vtfs_feature_desc);

	error = vtfs_setup_features(sc);
	if (error) {
		device_printf(dev, "cannot setup features\n");
		goto fail;
	}

	error = vtfs_alloc_hiprio_virtqueue(sc);
	if (error) {
		device_printf(dev, "cannot allocate high priority virtqueue\n");
		goto fail;
	}

	error = vtfs_alloc_req_virtqueue(sc);
	if (error) {
		device_printf(dev, "cannot allocate request virtqueue\n");
		goto fail;
	}

	return(1);

fail:
	if (error)
		vtfs_detach(dev);

	return (error);
}

static int
vtfs_detach(device_t dev)
{
	struct vtfs_softc *sc;

	sc = device_get_softc(dev);

	return (1);
}

static void
vtfs_negotiate_features(struct vtfs_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtfs_dev;
	features = vtfs_FEATURES;

	sc->vtfs_features = virtio_negotiate_features(dev, features);
}

static int
vtfs_alloc_hiprio_virtqueue(struct vtfs_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info;

	dev = sc->vtfs_dev;

	VQ_ALLOC_INFO_INIT(&vq_info, 0, NULL, sc, &sc->vtfs_hipriovq,
	    "%s request", device_get_nameunit(dev));

	return (virtio_alloc_virtqueues(dev, 0, 1, &vq_info));
}

static int
vtfs_alloc_req_virtqueue(struct vtfs_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info;

	dev = sc->vtfs_dev;

	VQ_ALLOC_INFO_INIT(&vq_info, 0, NULL, sc, &sc->vtfs_reqvq,
	    "%s request", device_get_nameunit(dev));

	return (virtio_alloc_virtqueues(dev, 0, 1, &vq_info));
}