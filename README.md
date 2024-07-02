# What is SCSI2Pi?

SCSI2Pi is the advanced and more performant alternative to the <a href="https://github.com/PiSCSI/piscsi">PiSCSI software</a> for the PiSCSI/RaSCSI board. SCSI2Pi provides an improved device emulation, higher transfer rates, new SCSI initiator mode tools and <a href="https://www.scsi2pi.net/en/piscsi_comparison.html">numerous new features</a>. SCSI2Pi is compatible with the PiSCSI web UI and the <a href="https://www.scsi2pi.net/en/app.html">SCSI Control app</a> for Android. With SCSI2Pi the app offers an extended set of features.<br />
SCSI2Pi emulates several SCSI or SASI devices like hard drives, CD-ROM drives, printers or network adapters at the same time. You can easily add a range of devices to computers like 68k Macs, Atari ST/TT/Falcon030, Amiga, Unix workstations, samplers or other computers with SCSI port.<br />
You can run SCSI2Pi stand-alone or eswitch from PiSCSI to SCSI2Pi in seconds, simply by installing a <a href="https://www.scsi2pi.net/en/downloads.html">package with highly optimized SCSI2Pi binaries</a>. No time-consuming compilation is required.<br />
The <a href="https://www.scsi2pi.net">SCSI2Pi website</a> addresses regular users and developers, whereas the information on GitHub is rather developer-centric.

# Who am I?

Until release 24.04.01 I was the <a href="https://www.scsi2pi.net/en/scsi2pi.html">main contributor</a> to the PiSCSI SCSI emulation. I revised the backend architecture, added a remote interface and re-engineered most of the legacy code so that it uses modern C++. This resulted in more modular code and drastically improved <a href="https://sonarcloud.io/project/overview?id=uweseimet_scsi2pi">SonarQube code metrics</a>. Besides adding numerous <a href="https://www.scsi2pi.net/en/scsi2pi.html">new features</a> and improving the compatibility with many platforms, I also fixed a range of bugs and added an extensive set of unit tests.<br />
I am also the author of the <a href="https://www.hddriver.net">HDDRIVER driver software for Atari computers</a> and the <a href="https://www.scsi2pi.net/en/app.html">SCSI Control app</a> for Android, which is the remote control for your PiSCSI/RaSCSI boards. SCSI Control supports both SCSI2Pi and PiSCSI. The full range of app features requires SCSI2Pi, though.

# How is SCSI2Pi related to PiSCSI?

In the PiSCSI project there was not much interest in replacing old, often buggy or unnecessary code, or to improve the data transfer rates. Long promised features on the roadmap and user requests in tickets were not addressed, and it took long for features or bug fixes to make it into a release. This is why I started to work on the emulation in a separate project, while staying compatible with the PiSCSI web interface. The major part of the PiSCSI C++ codebase has been contributed by me anyway.<br />
With PiSCSI there was also not much interest in further developing the SCSI emulation and exploiting the initiator mode feature of the FULLSPEC board. This mode, together with new SCSI2Pi command line tools, offers solutions for use cases that have never been addressed before. These tools also help with advanced testing, making the emulation more robust and reliable.

# SCSI2Pi goals

SCSI2Pi is not meant to completely replace the PiSCSI software, but only the device emulation and the tools. For the PiSCSI project great work is still being done on the web interface, and on supporting users in social media.<br />
There is no SCSI2Pi support for the X68000 platform, in particular not for the host bridge (SCBR) device. In PiSCSI the respective code has always been in a bad shape, and nobody has been interested in testing it. The other PiSCSI features (and many more) are supported and often improved by SCSI2Pi.
