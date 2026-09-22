@ECHO OFF
CLS
"C:\Program Files (x86)\Microsoft Visual Studio\VB98\Wizards\RC.EXE" /r /fo BalloonTooltips.res BalloonTooltips.rc
"C:\Program Files (x86)\Microsoft Visual Studio\VB98\VB6.EXE" prjBalloonTooltips.vbp /make
PAUSE