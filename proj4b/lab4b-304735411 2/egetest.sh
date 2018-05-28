#!/bin/sh

echo | ./lab4b --bogus &> /dev/bull;
if [[$? -ne 1]]
then
	echo "Fail: bad argument doesn't return proper exit code"
else
	echo "Passed test for bad argument"
fi

if [-f logfile.txt ]
then
	rm logfile.txt
fi

{ sleep 5; echo "STOP"; sleep 4; echo "OFF";} | ./lab4b --log=logfile.txt --period=4

if [ ! -f logfile.txt ]
then
	echo "Fail: logfile not created"
else
	echo "Passed test for logfile creation"
fi

grep "[0-2][0-9]:[0-5][0-9]:[0-5][0-9] [4-9][0-9].[0-9]" logfile.txt > grep.txt

if [ ! $(wc -l < grep.txt) -eq 3 ]
then
    echo "Fail: doesn't read temperature properly for fahrenheit"
else
	echo "Passed test for temperature reading"
fi

grep "STOP" logfile.txt > grep.txt
if [ $? != 0 ]
then
	echo "Fail: didn't  write command to log"
else
	echo "Passed test for writing to log"
fi

grep "OFF" logfile.txt > grep.txt
if [ $? != 0 ]
then
	echo "Fail: didn't process command while temperature is not being recorded"
else
	echo "Passed test for recording temperature when not recording temperature"
fi

grep "[0-2][0-9]:[0-5][0-9]:[0-5][0-9] SHUTDOWN" logfile.txt > grep.txt
if [ $? != 0 ]
then
	echo "Fail: didn't write time with shutdown message"
else
	echo "Passed test case for shutdown message"
fi

echo "LOGFILE"
cat logfile.txt
echo "END OF LOGFILE"
rm grep.txt
rm logfile.txt
