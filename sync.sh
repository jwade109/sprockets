TARGET=$1
TARGET_PATH=$2

scp *.c Makefile valgrind.sh $TARGET:$TARGET_PATH
ssh $TARGET "cd $TARGET_PATH; make node"
