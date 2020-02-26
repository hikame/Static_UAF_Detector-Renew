LEVEL = ../..
SRCS = foo.c
include $(LEVEL)/Makefile.inc

test:
	kmdriver -bbAnalyzeLimit 2 -cplimit 10 -ctn false -printTC true -printER true -printDB true -printWN true -tm EVP_PKEY_CTX_free -suaf -o "$(PWD)" "$(PWD)/merged.bc"
	kmdrawer -e "$(PWD)" ./
	kmdrawer -s -e "$(PWD)" ./
