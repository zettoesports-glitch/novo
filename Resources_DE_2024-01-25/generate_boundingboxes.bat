@echo off
cd ./data
for /f "usebackq delims=" %%f in (`dir /s /b *.bmd`) do (
	echo "Processing %%f"
	call "..\x64\GenerateBoundingBox" "%%f"
)