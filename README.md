# rendervid
A simple, streamlined C++ library to get video files into a 3D renderer.
The code that is new for this repository is licensed under the MIT License.
Dependent libraries (Ogg, Vorbis, Theora, VPX) have their own separate licenses.

The Ogg Theora code in this library is essentially good to go as far as video decoding.
Take a look in the TheoraPlayer folder for everything you need. Player.cpp and TheoraPlayer.sln is a usage example.
Audio is not implemented at this time.

WebM/VPX video decoding is working in the vpxtest folder, on top of Mozilla's NestEgg.
It is not wrapped into any kind of neat library at this time.
Audio is not implemented at this time.