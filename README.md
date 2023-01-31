# Multi-flow device file

## Specifica
----

This specification is related to a Linux device driver implementing low and
high priority flows of data. Through an open session to the device file a thread
can read/write data segments. The data delivery follows a First-in-First-out
policy along each of the two different data flows (low and high priority). After
read operations, the read data disappear from the flow. Also, the high priority data 
flow must offer synchronous write operations while the low priority
data flow must offer an asynchronous execution (based on delayed work)
of write operations, while still keeping the interface able to synchronously
notify the outcome. Read operations are all executed synchronously. The
device driver should support 128 devices corresponding to the same amount
of minor numbers.
The device driver should implement the support for the _ioctl(..)_ service
in order to manage the I/O session as follows:

* setup of the priority level (high or low) for the operations
* blocking vs non-blocking read and write operations
* setup of a timeout regulating the awake of blocking operations

A few Linux module parameters and functions should be implemented in
order to enable or disable the device file, in terms of a specific minor number.
If it is disabled, any attempt to open a session should fail (but already open
sessions will be still managed). Further additional parameters exposed via
VFS should provide a picture of the current state of the device according to
the following information:

* enabled or disabled
* number of bytes currently present in the two flows (high vs low priority)
* number of threads currently waiting for data along the two flows (high vs low priority)

## Istruzioni per inserimento e rimozione del modulo kernel.
----

```bash
# compilazione del modulo kernel (nella directory di multi_flow.c)
make all

# clean dei file
make clean

# inserimento del modulo kernel
sudo insmod multi_flow.ko

# rimozione del modulo kernel
sudo rmmod multi_flow.ko
```
## Istruzioni per la compilazione e per il lancio dell'applicazione utente.
----

```bash
# compilazione (nella directory di user.c)
make all

# lancio del programma (major number può differire) 
sudo ./user /dev/my-device <major> <minor>
```

## Visualizzazione dei parametri del modulo kernel (come da specifica).
----

```bash
sudo cat /sys/module/multi_flow/parameters/disabled_device
sudo cat /sys/module/multi_flow/parameters/hp_bytes
sudo cat /sys/module/multi_flow/parameters/lp_bytes
sudo cat /sys/module/multi_flow/parameters/hp_threads
sudo cat /sys/module/multi_flow/parameters/lp_threads
```