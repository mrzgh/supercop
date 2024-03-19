SUPERCOP
===

SUPERCOP is a toolkit developed by the VAMPIRE lab for measuring the performance of cryptographic software. SUPERCOP stands for System for Unified Performance Evaluation Related to Cryptographic Operations and Primitives; the name was suggested by Paul Bakker.
The latest release of SUPERCOP measures the performance of hash functions, secret-key stream ciphers, public-key encryption systems, public-key signature systems, and public-key secret-sharing systems. SUPERCOP integrates and improves upon

STVL's benchmarking suite for stream ciphers submitted to eSTREAM, the ECRYPT Stream Cipher Project (which finished in April 2008);
VAMPIRE's BATMAN (Benchmarking of Asymmetric Tools on Multiple Architectures, Non-interactively) suite for public-key systems submitted to the eBATS (ECRYPT Benchmarking of Asymmetric Systems) project; and
additional tools developed for VAMPIRE's new eBASH (ECRYPT Benchmarking of All Submitted Hashes) project.
Specifically, SUPERCOP measures cryptographic primitives according to several criteria:

Time to hash a very short packet of data.
Time to hash a typical-size Internet packet.
Time to hash a long message.
Length of the hash output.
Time to encrypt a very short packet of data using a secret key and a nonce.
Time to encrypt a typical-size Internet packet.
Time to encrypt a long message.
Length of the secret key.
Length of the nonce.
Time for authenticated encryption of a short packet of data.
Time for authenticated encryption of a typical-size Internet packet.
Time for authenticated encryption of a long message.
Time to generate a key pair (a private key and a corresponding public key).
Length of the private key.
Length of the public key.
Time to generate a shared secret from a private key and another user's public key.
Length of the shared secret.
Time to encrypt a message using a public key.
Length of the encrypted message.
Time to decrypt a message using a private key.
Time to sign a message using a private key.
Length of the signed message.
Time to verify a signed message using a public key.
"Time" refers to time on real computers: time on an ARM Cortex-A8, time on an Intel Sandy Bridge, time on an Intel Haswell, etc. The point of these cost measures is that they are directly visible to the cryptographic user.
Contributing computer time to benchmarking

Do you have a computer that has enough time to benchmark all the available cryptographic software, that has no other tasks consuming CPU power, and that will have time in the future for updated benchmarks? Would you like to contribute CPU cycles to benchmarking? Perhaps your favorite type of computer isn't included in the current list of benchmarking platforms. Even if all of your computers are similar to computers in the list, you can help by providing independent verification of the speed measurements.
To collect measurements, simply download, unpack, and run SUPERCOP:

     wget https://bench.cr.yp.to/supercop/supercop-20240107.tar.xz
     unxz < supercop-20240107.tar.xz | tar -xf -
     cd supercop-20240107
     nohup sh do &
Put the resulting supercop-20240107/bench/*/data.gz file on the web, and send the URL to the eBACS/eBATS/eBASC/eBASH mailing list.
Your computer's clock needs to be reasonably accurate. You need standard compilation tools (apt install build-essential on Debian). You should also install xsltproc (apt install xsltproc) to compile the Keccak Code Package, and OpenSSL (apt install libssl-dev) to benchmark implementations that use OpenSSL, but if these aren't installed then SUPERCOP will still benchmark other implementations.

Multiple computers that share filesystems can run SUPERCOP in the same directory. Each computer will create its own subdirectory of bench, labelled by the computer's name, and will perform all work inside that subdirectory.

SUPERCOP is a very large package, open to public contributions. It includes and uses code from questionable sources such as NSA. You should not run it on machines that have confidential data, special network access, etc.

Alternative: Incremental benchmarks

Here is a different method of collecting measurements:
     wget https://bench.cr.yp.to/supercop/supercop-20240107.tar.xz
     unxz < supercop-20240107.tar.xz | tar -xf -
     cd supercop-20240107
     nohup sh data-do &
Put the resulting supercop-data/*/data.gz file on the web, and send the URL to the eBACS/eBATS/eBASC/eBASH mailing list.
The disadvantage of this method is that it consumes extra disk space (typically 20 gigabytes or more, and many inodes). The big advantage of this method is incrementality: an updated version of SUPERCOP will automatically reuse most of the work from the supercop-20240107 run, benchmarking only new code and changed code, so the new benchmark run will finish much more quickly. (However, if an OS update has changed the compiler version, everything will be automatically re-benchmarked.)

Another advantage of this method is parallelizability: on (e.g.) a 4-core machine you can run

     nohup sh data-do 4 &
to finish the benchmarks almost 4 times as quickly.
Reducing randomness in benchmarks

Heterogeneous cores. ARM big.LITTLE systems have two different types of cores supporting the same instructions. Depending on the CPU load, the OS may decide to run jobs on the smaller, lower-power cores or on the larger, faster cores. It is also possible, although less common, to have heterogeneous cores on Intel/AMD systems.

Running ./data-do 4 automatially pins tasks to cores 0, 1, 2, 3, but how do you figure out that cores 0, 1, 2, 3 all have the same type? And how do you benchmark the other types of cores? The simple answer is to instead run ./data-do-biglittle (without a number). This script figures out which different types of cores there are, and separately benchmarks each type. The script should work on any Linux system with Python and with enough CPU data in /sys/devices/system/cpu and /proc/cpuinfo; if in doubt, try running ./data-do-biglittle -d first for a quick overview of which cores it will use.

Interrupts and hyperthreading. SUPERCOP checks the CPU's cycle counter, runs a computation, checks the cycle counter again, runs the computation again, etc. It records all the resulting measurements. It also runs the measuring program multiple times in case there are variations from one run to another (from, e.g., randomized memory layouts). Medians and quartiles are reported on the web pages, and any severe discrepancies are flagged in red.

An interrupt (for an incoming network packet, for example, or for an operating-system clock tick) will slow down a computation. This is much less important for SUPERCOP than it is for most benchmarking tools: a typical cryptographic operation measured by SUPERCOP completes quickly and is unlikely to be interrupted.

However, the picture changes if interrupts are frequent enough to interrupt 25% or more of the computations. The most common way for this to happen is for the same CPU core to be busy doing something else. It is thus recommended to run SUPERCOP on an idle machine.

On an otherwise idle machine, ./data-do 4 can interfere with itself, because what the OS calls cores 0, 1, 2, 3 is actually 4 "hyperthreads" competing for resources on just 2 cores. On typical Linux machines, hyperthreading is indicated by dashes or commas in the files in /sys/devices/system/cpu/cpu*/topology/thread_siblings_list: for example, if cpu2 and cpu6 both say 2,6 then they are actually one physical core running two threads.

./data-do-biglittle automatically checks for hyperthreading and runs on separate cores. An alternative is to turn off hyperthreading in the BIOS.

Hyperthreading can gain performance for some mixes of applications. It would be interesting, although challenging, to robustly benchmark this effect.

Underclocking and overclocking. A CPU that nominally runs at, e.g., 2GHz will often try to save power by running at a lower speed if it doesn't think it's busy enough to justify running at 2GHz. A computation then takes longer. A user who isn't satisfied with the resulting performance can and does tell the CPU to avoid this slowdown, so eBACS policy is to measure performance without this slowdown.

Note that the slowdown is not necessarily proportional to CPU frequency: for example, if DRAM continues running at full speed then a computation that accesses DRAM can end up taking fewer CPU cycles. Furthermore, it is not safe to assume that SUPERCOP creates enough load to avoid underclocking.

Inadequate cooling might force a CPU to reduce speed so as to avoid overheating. Standard cooling systems are designed so that the CPU can always run at its nominal frequency without overheating. Machines where cooling problems are detected are marked as unstable in eBACS tables.

The same CPU might also sometimes be able to run at a higher speed: "Turbo Boost", "Turbo Core", etc. The reason the CPU's nominal speed isn't higher is that the CPU can't always run at a higher speed: some computations will make the CPU overheat with standard cooling systems or, even worse, draw too much power. The power and temperature depend on the mix of current and recent computations, requiring lengthy benchmarks for accurate measurements and raising the question of what mixes are appropriate.

The current goal of eBACS is limited to measuring performance at the nominal CPU frequency. This is not meant to understate the importance of understanding the CPU frequencies that can be achieved in various environments, the performance of cryptographic software at higher frequencies, and the ability of software to sustain those frequencies.

Given the above considerations, it is recommended that SUPERCOP users disable underclocking, disable overclocking, and check for adequate cooling.

On current Linux systems, the system administrator can disable overclocking with

     echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
on Intel systems or

     echo 0 > /sys/devices/system/cpu/cpufreq/boost
on AMD systems. This lasts until the next boot. For an effect that persists across reboots, the system administrator can do the same in boot scripts. An alternative is to disable overclocking in the BIOS.

On current Linux systems, the system administrator can disable underclocking as follows (unless the BIOS is configured to limit OS control):

     for i in /sys/devices/system/cpu/cpu*/cpufreq
     do
       echo performance > $i/scaling_governor
     done
Some CPU-OS combinations provide more fine-grained control, such as

     for i in /sys/devices/system/cpu/cpu*/cpufreq
     do
       echo userspace > $i/scaling_governor
       echo 2000000 > $i/scaling_setspeed
     done
to set a fixed 2GHz frequency. On these machines, you can see the available frequencies listed in /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies. When the top two frequencies are separated by just 1000, the larger is Turbo Boost; use the smaller.

Beware that it is common for an OS to enable underclocking starting on the scale of a minute after boot, overriding a boot script that disables underclocking. The system administrator can disable the enable-underclocking script for future reboots, but details depend on the OS; update-rc.d ondemand disable works on some systems, and systemctl disable ondemand.service works on some systems.

Low-precision clocks and mis-timed clocks. It is rare for a CPU to not provide a cycle counter. It is, however, common for a CPU to make its cycle counter available only to the OS kernel, at least by default. If the best clock that SUPERCOP can find ticks only 1000000 times per second, while the computation of interest finishes in 1.5 microseconds, then SUPERCOP's measurements will have tremendous variation: 1 microsecond, then 2 microseconds, then 1 microsecond, then 2 microseconds, etc. SUPERCOP could increase the precision by measuring the clock only after, say, 100 copies of the computation, but this would hide other variations of interest and would make the measurements less robust against frequent OS interrupts. It is better to give SUPERCOP access to a cycle counter.

On Intel/AMD systems, SUPERCOP generally uses the RDTSC instruction. This instruction has always been documented to count clock cycles since CPU reset, and normally the OS makes it available to applications by default. Intel and AMD now creatively interpret "clock cycles" to mean not CPU cycles, but rather the cycles of an artificial clock always running at the nominal CPU frequency; this discrepancy does not matter if the CPU is running at the nominal CPU frequency. Beware that some Intel CPUs run the artificial clock at a different frequency from the nominal CPU frequency.

Starting in 2021, SUPERCOP favors the RDPMC instruction on Linux systems that make RDPMC available to applications. The cycle counter provided through RDPMC runs at the actual CPU frequency, so accidental variations in the CPU frequency have much less effect on RDPMC results than on RDTSC results. There is still an effect from accesses to off-CPU resources such as full-speed DRAM, so controlling the CPU frequency is still recommended.

If /proc/sys/kernel/perf_event_paranoid says 3 then the system administrator will need to set it to 2 to enable RDPMC. People who disable RDPMC are generally reacting to side-channel attacks that use RDPMC, but there is little reason to believe that the variable-time software targeted by those attacks would be secure against a competent attacker armed only with RDTSC or other clocks.

Xen systems similarly disable RDPMC by default, but it may be possible to enable RDPMC with vpmu=on.

On current ARM systems under Linux, the following kernel modules enable the cycle counter on all cores (change SUBDIRS= to M= in ko/Makefile to compile with kernel versions after 5.3): https://github.com/thoughtpolice/enable_arm_pmu (32-bit) and https://github.com/rdolbeau/enable_arm_pmu (64-bit). More variations are required for older systems, such as the following:

Sharp NetWalker PC-Z1, 800MHz Freescale i.MX515 (ARM Cortex A8), Ubuntu 9.04
SheevaPlug, 1200MHz Marvell Kirkwood 88F6281 (ARM926EJ-S), Ubuntu 9.04
Nokia N810, 400MHz TI OMAP 2420 (ARM1136J), Maemo Diablo
Further variations. Some cryptographic computations naturally vary in run time. An obvious example is RSA key generation, using rejection sampling to generate primes. In theory SUPERCOP could perform these computations enough times to see that their time follows a stable distribution, and could then remove the red flags.

Most computations run more slowly the first time because the code needs to be loaded into cache. SUPERCOP does not attempt to measure this effect (the effect disappears in medians and quartiles), but it has begun recording code size, which puts an upper bound on the effect.

There are many other microarchitectural effects that create variations from one run of a computation to another run of the same computation, even when the inputs and randomness are identical: e.g., branch-predictor state.

Database format

The output of SUPERCOP is an extensive database of measurements in a form suitable for easy computer processing. The database is currently stored as a separate compressed data.gz file for each machine. Version 20100702 of SUPERCOP, on a typical 2.4GHz Core 2 (two architectures, amd64 and x86, but with only 64-bit OpenSSL), produces a 94-megabyte data.gz that uncompresses to 734 megabytes.
The database, in uncompressed form, consists of a series of database entries. Each database entry is a line consisting of the following space-separated words:

SUPERCOP version; e.g., 20100702.
Computer name; e.g., utrecht. There is a separate page providing more information about the computers: e.g., utrecht's CPU is a 2400MHz Intel Core 2 Quad Q6600 (6fb). Benchmarks on multiple-CPU machines use just one CPU, and benchmarks on multiple-core CPUs use just one core.
Application Binary Interface (ABI); e.g., amd64. On a computer that supports multiple incompatible ABIs (e.g., 32-bit x86 and 64-bit amd64), SUPERCOP automatically collects separate measurements for each ABI. Beware that the ABI names are not standardized.
Benchmark start date; e.g., 20100703.
Operation (type of primitive) measured; e.g., crypto_hash.
Primitive measured; e.g., sha256. In SUPERCOP releases starting 2020.08, sha256/constbranchindex is for measurements of sha256 implementations that report goal-constbranch and goal-constindex, and sha256/timingleaks is for other measurements. Further extensions are possible.
Additional words giving details of the measurements.
There are also database entries whose first word is a plus sign. These entries are meant for human consumption and are not in a documented format.
SUPERCOP automatically tries all available implementations of each primitive, and many compilers for each implementation, to select the fastest combination of implementation and compiler. Each try produces a database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word try.
A checksum of various outputs of the implementation; e.g., 86df8bd202b2a2b5fdc04a7f50a591e43a345849c12fef08d487109648a08e05.
The word ok if the checksum is correct, or fails if the checksum is incorrect, or unknown if the correct checksum is not known. An implementation+compiler combination that produces an incorrect checksum is skipped.
The number of cycles used for a typical cryptographic operation; e.g., 35289. This is actually the median of many measurements. SUPERCOP selects the implementation+compiler combination that minimizes this number. For example, for hash functions, SUPERCOP selects the implementation+compiler combination that minimizes the time to hash 1536 bytes.
The number of cycles used for computing the checksum; e.g., 220716990.
The number of cycles per second; e.g., 2405453000.
The implementation used; e.g., crypto_hash/sha256/openssl.
The compiler used; e.g., gcc_-m64_-march=k8_-O3_-fomit-frame-pointer.
If a compiler issues an error message (or a warning or any other output), SUPERCOP produces a database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word fromcompiler.
The implementation used; e.g., crypto_hash/shavite3512/lower-mem.
The compiler used; e.g., gcc_-march=nocona_-Os_-fomit-frame-pointer.
The file being compiled; e.g., SHAvite3.c.
One or more words of output repeating the error message: e.g., portable.h:109:2: warning: #warning NEITHER NESSIE_LITTLE_ENDIAN NOR NESSIE_BIG_ENDIAN ARE DEFINED!!!!! Several error messages will produce several database entries (in the same order).
If an implementation fails to run (for example, because it uses machine instructions not supported by the CPU), SUPERCOP produces a database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word tryfails.
The implementation used; e.g., crypto_hash/fugue256/SSE4.1.
The compiler used; e.g., gcc_-m64_-march=core2_-msse4_-O3_-fomit-frame-pointer.
One or more words of output describing the failure; e.g., Illegal instruction. Several lines of output will produce several database entries (in the same order).
SUPERCOP then measures the performance of the selected implementation and compiler on a wider variety of specific operations; for example, hash functions are selected on the basis of 1536-byte hashing, but are then measured for hashing 0 bytes, 1 byte, 2 bytes, 3 bytes, etc. Each specific operation produces a database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
One of the following words:
cycles: generating a shared secret (crypto_dh), hashing (crypto_hash), encrypting a message under a public key (crypto_encrypt), signing a message (crypto_sign), or generating a stream from a secret key (crypto_stream);
encrypt_cycles: currently undocumented;
decrypt_cycles: currently undocumented;
forgery_decrypt_cycles: currently undocumented;
afternm_cycles: currently undocumented;
base_cycles: currently undocumented;
beforenm_cycles: currently undocumented;
keypair_cycles: generating a key pair (crypto_dh_keypair or crypto_encrypt_keypair or crypto_sign_keypair);
forgery_open_afternm_cycles: currently undocumented;
forgery_open_cycles: currently undocumented;
open_afternm_cycles: currently undocumented;
open_cycles: decrypting a message (crypto_encrypt_open) or verifying a signed message (crypto_sign_open);
verify_cycles: currently undocumented;
xor_cycles: encrypting a message under a secret key (crypto_stream_xor);
bytes: the length of an encrypted or signed message;
beforenmbytes: currently undocumented;
boxzerobytes: currently undocumented;
bssbytes: the number of bytes in the bss section;
constbytes: currently undocumented;
databytes: the number of bytes in the data section;
inputbytes: currently undocumented;
keybytes: currently undocumented;
noncebytes: currently undocumented;
open_bytes: the length of a decrypted or verified message;
outputbytes: currently undocumented;
publickeybytes: currently undocumented;
scalarbytes: currently undocumented;
secretkeybytes: currently undocumented;
statebytes: currently undocumented;
textbytes: the number of bytes in the text section;
zerobytes: currently undocumented.
The number of original message bytes hashed, encrypted, etc.; e.g., 96.
The median of many successive measurements; e.g., 3159.
The first measurement; e.g., 4131.
The second measurement; e.g., 3222.
...
The last measurement; e.g., 3159.
SUPERCOP also records the selected implementation in a separate database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word implementation.
The implementation used; e.g., crypto_hash/sha256/openssl.
The implementation version (the CRYPTO_VERSION macro) or - if no version number is defined by the implementation.
SUPERCOP also records the selected compiler in a separate database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word compiler.
The compiler used; e.g., g++_-m64_-march=nocona_-O2_-fomit-frame-pointer.
The compiler version; e.g., 4.3.3.
SUPERCOP also records the CPU identifier in a separate database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word cpuid.
The CPU identifier; e.g., GenuineIntel-000006fb-bfebfbff_.
SUPERCOP also records the number of CPU cycles per second in a separate database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word cpucycles_persecond.
The number of CPU cycles per second; e.g., 2394000000.
SUPERCOP also records the cycle-counting mechanism in a separate database entry with the following words:

SUPERCOP version.
Computer name.
ABI.
Benchmark start date.
Operation measured.
Primitive measured.
The word cpucycles_implementation.
The mechanism; e.g., amd64cpuinfo.
Version

This is version 2024.01.07 of the supercop.html web page. This web page is in the public domain.
