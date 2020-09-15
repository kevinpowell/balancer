# A hacked together single/dual plane balancer (for cheap)

## System components:
  - *Rasberry pi 4
  - Prototyping hat for rpi 4
  - MCP3008 adc
  - IR LED/photoresistor proximity detector
  - 2 displacement to volatage transducers (initial system used small speakers)


## Assembly:
  - Install RPi OS and upgrade to latest packages
    - Test system used custom 5.6.19 kernel with RT patchset
  - Connect MCP3008 to spi0 on rpi
  - Connect transducer 1 to first differential channel on mcp
  - Connect transducer 2 to second differential channel on mcp
  - Connect IR sensor output to ch7 on mcp

## Configure System:
  - Enable spi: add "dtparam=spi=on" to config.txt
  - Enable mcp iio driver: add "dtoverlay=mcp3008:spi0-0-present,spi0-0-speed=18000000" to config.txt
  - Build pcmcnt_mod kernel module and arrange for it to be loaded at boot
  - Set the rpi frequency governor to performance and pin the cpu freq to 1.5GHz
  - Build the collect_3008 executable (via make) and arrange for it to be in your path
  - Install the python module (written for Python3 only!) in whatever way seems best (e.g. python setup.py develop --user)

## Software Overview:
  There are three parts to the software system: small kernel module, C code to capture samples, Python code for data processing

  The small kernel module merely enables userspace access to the cycle counter (which is used by the daq code)

  The C code gathers samples from the adc (3 channels), timestamps them, and saves them in a file.  C code is futher decomposed
  into a main loop handling timing (main_timer.c) and device specific code for the mcp3008 (collect_3008.c).  collect_3008.c
  uses the iio sysfs interface to the mcp3008 kernel driver to get adc samples from the chip.

  The Python module (cheapskate_bal) uses a combination of pandas, scipy, and numpy to process the collected data and generate
  suggested correction weights.  The python module also defines a few high level entry points that attempt to orchestrate the
  data collection process.

## System Use:
  - attach transducers to DUT at supporting bearing(s).  rig the IR prox sensor to trip once per revolution of the DUT.
  - for single plane balance: csbal_s \<stem> \<freq> \<shift_ang> \<test_mass>
    - where
      - stem is a file stem e.g data/fan1/  (if it is a directory, it must exist or the script will crash)
      - freq is the frequency of interest (rpm setting of DUT/60)
      - shift_ang is the distance in degrees from the reference line where the test mass is mounted
      - test_mass is the mass of the test mass used. correction weights will have the same units as this input

  - for the initial dual plane balance: csbal_dinit \<stem> \<freq> \<shift_ang> \<test_mass> 
      - note that the script assumes that the A and B trials will use the same mass and shift_ang

  - for subsequent dual plane balance: csbal_d \<stem> \<tag> \<freq>
      - where 
        - tag is a 'file tag' the data for this iteration will be stored at stem / t\<tag>-ch[x].csv

## Acknowledgements:
  Too many to list.  This project is largely cobbled together from sources on the internet.  I am particularly
  indebted to Conrad Hoffman (for the math behind the single plane mode).  See his website at http://www.conradhoffman.com/chsw.htm.
  The math for the dual plane balancer came from https://www.maintenance.org/fileSendAction/fcType/0/fcOid/399590942964042178/filePointer/399590942964815332/fodoid/399590942964815330/TwoPlaneBalanceTrialATrialB_Maple.PDF 
  The outline for the data acquision part came from https://jumpnowtek.com/rpi/Using-mcp3008-ADCs-with-Raspberry-Pis.html
  main_timer.c was hoisted from https://stackoverflow.com/questions/24051863/how-to-implement-highly-accurate-timers-in-linux-userspace and subseqently beaten with the ugly stick.
  To the rest (and there are many) I'm sorry I forgot where your snippets came from.


## General Notes/Future Plans:
  This software has the defecta trifecta: it is poorly written, it is poorly documented, and it is poorly supported.  Good luck to you if you try it.

  I'll consider pull requests in my own time, thanks.

  The idea for using speakers as transducers came from Hoffman.  Later, I discovered that sparkfun (and others) have very affordable piezo sensors. They're probably better.

  I conected the speakers to the adc in the most naive possible way.  It would be better to have proper biasing via op-amps.

  The MCP3008 is plenty fast for this operation, but a more modern adc with a gain stage would probably be preferable.  A real voltage reference would not go amiss, either.
  The cheapo IR proximity detectors that I used for sensing the positon of the rotor don't have the response time they should (could be my underdeveloped circuit building, though)

  In the early stages of this project, I made several errors.  One of which led me to believe that I would need to support 20ksamps/sec. That's why I used the RT kernel and
  it also explains all the hamfisted signal handling action in main_timer.  I would have been better off spending time updating the mcp3008 kernel driver to use iio triggering
  and buffering.  Speaking of which, in the wip directory there is a data collector that is based on libiio.  I never got it working, but the guts are sort of there.

  I got surprisingly close to the 20ksamps/sec that I set out to get (actually 60k, since there are 3 chans) Unfortunately in the read_adc function there are two syscalls (read,lseek)
    per channel.
  That pretty much torpedoes the top speed.  I think that splice or copy_file_range might make the difference.  One could splice the data from the adc filehandle to a pipe, and then 
  drain the pipe in the main time where it calls pause().

  Note that the collect_3008 must be constrained to run on just one core.  the cycle count register used for timestamping is per-core.

  I tried running collect_3008 as a realtime process, but it caused a lot of trouble.  It also *reduced* the throughput.  It did reduce the timing jitter a lot, though.
