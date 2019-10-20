with import <nixpkgs> {};
stdenv.mkDerivation {
  name = "ft232h-eeprom";
  buildInputs = [ libftdi1 libusb ];
}
