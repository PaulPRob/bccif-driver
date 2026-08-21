Contents

DRIVER: Contains the linux software driver for the PCI BCC card
        Tested under kernel 2.4, but should work under 2.2 and
	2.6

bcc_test: A menu driven program that talks to the driver and 
          allows hw based debugging and reading/writing to the external bus

tess_app: Is a small test program that was used for development, it
          does a complete read/write test on IF1 and then a continous
          read/write test on IF2