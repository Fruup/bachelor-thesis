..\vendor\ffmpeg\ffmpeg.exe -i "..\screenshots\screenshot_%d.bmp" -r 60 -c:v libx264 -preset slow -crf 21 video.mp4
pause
