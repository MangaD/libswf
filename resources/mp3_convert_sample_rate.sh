#! /bin/bash

# Taken from https://www.linuxquestions.org/questions/linux-desktop-74/cmd-line-prog-to-convert-mp3-samplerate-503359/

for f in *.mp3; do
	rate=`mp3info -p "%q" "$f"`

	if [[ $rate != "44" ]]
	then
		title=`mp3info -p "%t" "$f"`
		track=`mp3info -p "%n" "$f"`
		artist=`mp3info -p "%a" "$f"`
		album=`mp3info -p "%l" "$f"`
		year=`mp3info -p "%y" "$f"`
		genre=`mp3info -p "%g" "$f"`
		comment=`mp3info -p "%c" "$f"`

		lame --resample 44.1 --tt "$title" --tn "$track" --ta "$artist" \
			--tl "$album" --ty "$year" --tg "$genre" --tc "$comment" \
			"$f" "$f.out"
		
		mv -f "$f.out" "$f"
	fi
done