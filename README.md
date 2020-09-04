# Generating random sounds with random opus packets

This is a c file that will create random instrument sounds by randomly generating an opus packet and playing it repeatedly. It has to use opus_decode_float since it has to control the gain of the sample manually. There might be a way to fiddle with the random sample to normalize the gain, but I do not know how.

[You can hear a sample of the output of this program here](https://soundcloud.com/blackle-mori/random-opus-packets)