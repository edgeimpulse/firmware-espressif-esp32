# Arm(R) Ethos(TM)-U core driver

This repository contains a device driver for the Arm(R) Ethos(TM)-U NPU.

## Building

The source code comes with a CMake based build system. The driver is expected to
be cross compiled for any of the supported Arm Cortex(R)-M CPUs, which requires
the user to configure the build to match their system configuration.


One such requirement is to define the target CPU, normally by setting
`CMAKE_SYSTEM_PROCESSOR`. **Note** that when using the toolchain files provided
in [core_platform](https://git.mlplatform.org/ml/ethos-u/ethos-u-core-platform.git),
the variable `TARGET_CPU` must be used instead of `CMAKE_SYSTEM_PROCESSOR`.

Target CPU is specified on the form "cortex-m<nr><features>", for example:
"cortex-m55+nodsp+nofp".

Similarly the target NPU configuration is
controlled by setting `ETHOSU_TARGET_NPU_CONFIG`, for example "ethos-u55-128".

The build configuration can be defined either in the toolchain file or
by passing options on the command line.

```[bash]
$ cmake -B build  \
    -DCMAKE_TOOLCHAIN_FILE=<toolchain> \
    -DCMAKE_SYSTEM_PROCESSOR=cortex-m<nr><features> \
    -DETHOSU_TARGET_NPU_CONFIG=ethos-u<nr>-<macs>
$ cmake --build build
```

or when using toolchain files from [core_platform](https://git.mlplatform.org/ml/ethos-u/ethos-u-core-platform.git)

```[bash]
$ cmake -B build  \
    -DCMAKE_TOOLCHAIN_FILE=<core_platform_toolchain> \
    -DTARGET_CPU=cortex-m<nr><features> \
    -DETHOSU_TARGET_NPU_CONFIG=ethos-u<nr>-<macs>
$ cmake --build build
```

## Driver APIs

The driver APIs are defined in `include/ethosu_driver.h` and the related types
in `include/ethosu_types.h`. Inferences can be invoked in two manners:
synchronously or asynchronously. The two types of invocation can be freely mixed
in a single application.

### Synchronous invocation

A typical usage of the driver can be the following:

```[C]
// reserve a driver to be used (this call could block until a driver is available)
struct ethosu_driver *drv = ethosu_reserve_driver();
...
// run one or more inferences
int result = ethosu_invoke(drv,
                           custom_data_ptr,
                           custom_data_size,
                           base_addr,
                           base_addr_size,
                           num_base_addr);
...
// release the driver for others to use
ethosu_release_driver(drv);
```

### Asynchronous invocation

A typical usage of the driver can be the following:

```[C]
// reserve a driver to be used (this call could block until a driver is available)
struct ethosu_driver *drv = ethosu_reserve_driver();
...
// run one or more inferences
int result = ethosu_invoke_async(drv,
                                 custom_data_ptr,
                                 custom_data_size,
                                 base_addr,
                                 base_addr_size,
                                 num_base_addr,
                                 user_arg);
...
// do some other work
...
int ret;
do {
    // true = blocking, false = non-blocking
    // ret > 0 means inference not completed (only for non-blocking mode)
    ret = ethosu_wait(drv, <true|false>);
} while(ret > 0);
...
// release the driver for others to use
ethosu_release_driver(drv);
```

Note that if `ethosu_wait` is invoked from a different thread and concurrently
with `ethosu_invoke_async`, the user is responsible to guarantee that
`ethosu_wait` is called after a successful completion of `ethosu_invoke_async`.
Otherwise `ethosu_wait` might fail and not actually wait for the inference
completion.

### Driver initialization

In order to use a driver it first needs to be initialized by calling the `init`
function, which will also register the handle in the list of available drivers.
A driver can be torn down by using the `deinit` function, which also removes the
driver from the list.

The correct mapping is one driver per NPU device. Note that the NPUs must have
the same configuration, indeed the NPU configuration can be only one, which is
defined at compile time.

## Implementation design

The driver is structured in two main parts: the driver, which is responsible to
provide an unified API to the user; and the device part, which deals with the
details at the hardware level.

In order to do its task the driver needs a device implementation. There could be
multiple device implementation for different hardware model and/or
configurations. Note that the driver can be compiled to target only one NPU
configuration by specializing the device part at compile time.

## Data caching

For running the driver on Arm CPUs which are configured with data cache, certain
caution must be taken to ensure cache coherency. The driver expects that cache
clean/flush has been done by the user application before being invoked. The
driver does provide a deprecated weakly linked function `ethosu_flush_dcache`
that could be overriden, causing the driver to cache flush/clean base pointers
marked in the flush mask before each inference. By default the flush mask is set
to only clean the scratch base pointer containing RW data (IFM in particular).
It is recommended to not implement this function but have the user application
make sure that IFM data has been written to memory before invoking an inference
on the NPU.

The driver also exposes a weakly linked symbol for cache invalidation called
`ethosu_invalidate_dcache`, that must be overriden when the data cache is used.
After starting an inference on the NPU, the driver will call this function to
invalidate the base pointers marked in the invalidation mask. By defaults it
invalidates the scratch base pointer keeping RW data, to ensure cache coherency
after the inference is done. The invalidation call is done before waiting for
the NPU to finish the inference so that depending on the network, the cycles
for invalidating the cache may be completely hidden (the CPU performs cache
invalidation before yielding while waiting for the NPU to finish).

Make sure that any base pointers marked for flush/invalidation is aligned to the
cache line size of your CPU, typically 32 bytes. While not implemented, to the
really advanced user aiming for maximum performance, it is theoretically
possible to tell the network compiler to align the IFM/OFM to cache line size,
and modify the driver so that only OFM data is invalidated (and if left to the
driver, only IFM data is cache cleaned/flushed). Due to the uncertainty of
tensor alignment, the driver only flushes/invalidates on base pointer level.

By default the cache flush- and invalidation mask is set to only mark the
default scratch base pointer (base pointer 1). For maximum flexibility, the
driver provides a function to modify the cache flush/invalidate masks called
`ethosu_set_basep_cache_mask`. This function sets the two 8 bit masks, one for
flush and one for invalidate, where bit 0 corresponds to base pointer 0, bit 1
corresponds to base pointer 1 etc. See `include/ethosu_driver.h` for more
information.

An example implementation for the weak functions, using CMSIS primitives could
look like below:

```[C++]
extern "C" {
// Deprecated - recommended to flush/clean in application code
// p must be 32 byte aligned
void ethosu_flush_dcache(uint32_t *p, size_t bytes) {
    SCB_CleanDCache_by_Addr(p, bytes);
}

// p must be 32 byte aligned
void ethosu_invalidate_dcache(uint32_t *p, size_t bytes) {
    SCB_InvalidateDCache_by_Addr(p, bytes);
}
}
```
The NPU contain memory attributes that should be set to match the settings used
in the MPU configuration for the memories used. See `NPU_MEM_ATTR_[0-3]` for
Ethos-U85 and the `AXI_LIMIT[0-3]_MEM_TYPE` for Ethos-U55/Ethos-U65 in
corresponding `src/ethosu_config_uX5.h` files.

## Mutex and semaphores

To ensure the correct functionality of the driver mutexes and semaphores are
used internally. The default implementations of mutexes and semaphores are
designed for a single-threaded baremetal environment. Hence for integration in
environemnts where multi-threading is possible, e.g., RTOS, the user is
responsible to provide implementation for mutexes and semaphores to be used by
the driver.

The mutex and semaphores are used as synchronisation mechanisms and unless
specified, the timeout is required to be 'forever'.

The driver allows for an RTOS to set a timeout for the NPU interrupt semaphore.
The timeout can be set with the CMake variable `ETHOSU_INFERENCE_TIMEOUT`, which
is then used as `timeout` argument for the interrupt semaphore take call. Note
that the unit is implementation defined, the value is shipped as is to the
`ethosu_semaphore_take()` function and an override implementation should cast it
to the appropriate type and/or convert it to the unit desired.

A macro `ETHOSU_SEMAPHORE_WAIT_FOREVER` is defined in the driver header file,
and should be made sure to map to the RTOS' equivalent of
'no timeout/wait forever'. Inference timeout value defaults to this if left
unset. The macro is used internally in the driver for the available NPU's, thus
the driver does NOT support setting a timeout other than forever when waiting
for an NPU to become available (global ethosu_semaphore).

The mutex and semaphore APIs are defined as weak linked functions that can be
overridden by the user. The APIs are the usual ones and described below:

```[C]
// create a mutex by returning back a handle
void *ethosu_mutex_create(void);
// lock the given mutex
int ethosu_mutex_lock(void *mutex);
// unlock the given mutex
int ethosu_mutex_unlock(void *mutex);

// create a (binary) semaphore by returning back a handle
void *ethosu_semaphore_create(void);
// take from the given semaphore, accepting a timeout (unit impl. defined)
int ethosu_semaphore_take(void *sem, uint64_t timeout);
// give from the given semaphore
int ethosu_semaphore_give(void *sem);
```

## Begin/End inference callbacks

The driver provide weak linked functions as hooks to receive callbacks whenever
an inference begins and ends. The user can override such functions when needed.
To avoid memory leaks, any allocations done in the ethosu_inference_begin() must
be balanced by a corresponding free of the memory in the ethosu_inference_end()
callback.

The end callback will always be called if the begin callback has been called,
including in the event of an interrupt semaphore take timeout.

```[C]
void ethosu_inference_begin(struct ethosu_driver *drv, void *user_arg);
void ethosu_inference_end(struct ethosu_driver *drv, void *user_arg);
```

Note that the `void *user_arg` pointer passed to invoke() function is the same
pointer passed to the begin() and end() callbacks. For example:

```[C]
void my_function() {
    ...
    struct my_data data = {...};
    int result = int ethosu_invoke_v3(drv,
                                  custom_data_ptr,
                                  custom_data_size,
                                  base_addr,
                                  base_addr_size,
                                  num_base_addr,
                                  (void *)&data);
    ....
}

void ethosu_inference_begin(struct ethosu_driver *drv, void *user_arg) {
        struct my_data *data = (struct my_data*) user_arg;
        // use drv and data here
}

void ethosu_inference_end(struct ethosu_driver *drv, void *user_arg) {
        struct my_data *data = (struct my_data*) user_arg;
        // use drv and data here
}
```

## License

The Arm Ethos-U core driver is provided under an Apache-2.0 license. Please see
[LICENSE.txt](LICENSE.txt) for more information.

## Contributions

The Arm Ethos-U project welcomes contributions under the Apache-2.0 license.

Before we can accept your contribution, you need to certify its origin and give
us your permission. For this process we use the Developer Certificate of Origin
(DCO) V1.1 (https://developercertificate.org).

To indicate that you agree to the terms of the DCO, you "sign off" your
contribution by adding a line with your name and e-mail address to every git
commit message. You must use your real name, no pseudonyms or anonymous
contributions are accepted. If there are more than one contributor, everyone
adds their name and e-mail to the commit message.

```[]
Author: John Doe \<john.doe@example.org\>
Date:   Mon Feb 29 12:12:12 2016 +0000

Title of the commit

Short description of the change.

Signed-off-by: John Doe john.doe@example.org
Signed-off-by: Foo Bar foo.bar@example.org
```

The contributions will be code reviewed by Arm before they can be accepted into
the repository.

In order to submit a contribution push your patch to
`ssh://<GITHUB_USER_ID>@review.mlplatform.org:29418/ml/ethos-u/ethos-u-core-driver`.
To do this you will need to sign-in to
[review.mlplatform.org](https://review.mlplatform.org) using a GitHub account
and add your SSH key under your settings. If there is a problem adding the SSH
key make sure there is a valid email address in the Email Addresses field.

## Security

Please see [Security](SECURITY.md).

## Trademark notice

Arm, Cortex and Ethos are registered trademarks of Arm Limited (or its
subsidiaries) in the US and/or elsewhere.
