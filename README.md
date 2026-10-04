# RTL-SDR DAB+ Client

Kodi PVR client for DAB+ digital radio, received with an RTL-SDR USB stick. The stick is either
connected directly (on platforms with libusb support) or through an
[rtl_tcp](https://osmocom.org/projects/rtl-sdr/wiki) server on another device.

DAB decoding is done by [DAB-Radio](https://github.com/williamyang98/DAB-Radio).

## USB access

The stick can be used directly on Linux, macOS and Windows (desktop). On all other platforms,
such as Android, iOS, tvOS and webOS, use an rtl_tcp server.

* **Linux:** The kernel's DVB-T driver claims the stick, block it with
  `echo 'blacklist dvb_usb_rtl28xxu' | sudo tee /etc/modprobe.d/blacklist-rtl28xxu.conf` and
  reconnect the stick. The user running Kodi needs access to the USB device, e.g. through the udev
  rules of your distribution's rtl-sdr package.
* **Windows:** Install the WinUSB driver for the stick with [Zadig](https://zadig.akeo.ie)
  ("Bulk-In, Interface (Interface 0)").
* **Android:** A stick connected to the device itself can be used through an app that runs an
  rtl_tcp server on the device, such as
  [RTL-SDR driver](https://play.google.com/store/apps/details?id=marto.rtl_tcp_andro). Set the
  add-on's server address to `127.0.0.1` and the port to the one used by the app.

## Build instructions

### Linux / macOS

1. `git clone --branch master https://github.com/xbmc/xbmc.git`
2. `git clone https://github.com/ksooo/pvr.rtlsdr.dabplus.git`
3. `cd pvr.rtlsdr.dabplus && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.rtlsdr.dabplus -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/build/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make`

### Distribution packages

Outside of Kodi's add-on build system, the dependencies are taken from the system. DAB-Radio,
viterbi and kissnet are not packaged by distributions; `-DENABLE_INTERNAL_DABRADIO=ON` builds them
along with the add-on, as the Debian packaging in `debian/` does.

### Unit tests

The unit tests are built along with the add-on unless cross compiling. Run them with `ctest` in
the add-on's build directory (`build/pvr.rtlsdr.dabplus-prefix/src/pvr.rtlsdr.dabplus-build`).

To run the receiver test against a real signal, set `DABPLUS_TEST_IQ_FILE` to a raw recording of
an ensemble, e.g. `rtl_sdr -f 178352000 -s 2048000 -g 30 -n 20480000 5C.iq`.

## Trademark

The DAB+ logo used in the add-on icon is a trademark of Mitteldeutscher Rundfunk (MDR), used
under the [WorldDAB terms of use for the DAB+ marks](https://www.worlddab.org/resources/marketing-dab-plus/dab-plus-logo).
It is not covered by the GPL license of this add-on.
