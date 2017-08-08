# First argument: the hard drive that needs partitioning.

HD=$1;
BEAR_BIN=../../build.x86_64/bin
sudo losetup /dev/loop0 $HD
sudo mkfs.msdos /dev/loop0
mkdir partition
sudo mount /dev/loop0 partition
sudo cp $BEAR_BIN/ls partition/ls
sleep 2
sudo umount partition
sleep 2 # so losetup -d won't fail
sync
#sudo losetup -d /dev/loop0
rmdir partition
