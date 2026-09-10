@echo off
cd ./data
for /f "usebackq delims=" %%f in (`dir /s /b terrain.json`) do (
	echo "Processing %%f"
	call "..\x64\GenerateNavMesh" "%%f"
)