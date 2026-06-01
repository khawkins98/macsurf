-- MacSurf build robot for CodeWarrior 8 on Mac OS 9.
--
-- Compile this in Script Editor as a STAY-OPEN APPLICATION and drop it in the
-- System Folder's "Startup Items" so it auto-launches on boot. It polls a
-- trigger file on the shared TRANSFER volume; when present it builds MacSurf,
-- captures the Errors & Warnings window to a log, writes a status + DONE
-- sentinel back onto TRANSFER (which the host reads after shutdown), and
-- deletes the trigger so the next request can be queued.
--
-- This is the headless build path: no mouse, no menu clicks — pure AppleEvents.
-- Verified verbs (Metrowerks "Extending the CodeWarrior IDE", Building suite):
--   open file / Set Current Target / Remove Object Code / Make Project with ExternalEditor
--   / Save Error Window As.  Make returns short-int result codes; errShell_MakeFailed = 5.
--
-- HFS paths use colon syntax. Rename the IDE app to "CodeWarrior IDE" (drop the
-- version suffix) so `tell application "CodeWarrior IDE"` binds.

property kProjectFile : "Dev machine:macsurf:MacSurf.mcp"
property kTarget : "MacSurf PPC"
property kShareVol : "TRANSFER:"
property kTrigger : "TRANSFER:BUILD_REQUEST"
property kErrLog : "TRANSFER:build-errors.txt"
property kStatus : "TRANSFER:build-status.txt"
property kDone : "TRANSFER:BUILD_DONE"

on run
	-- build immediately on launch if a request is already staged, else idle-poll
	if fileExists(kTrigger) then doBuild()
end run

on idle
	if fileExists(kTrigger) then doBuild()
	return 3 -- re-check every 3 seconds
end idle

on doBuild()
	-- clear any prior sentinels
	rmIfExists(kDone)
	rmIfExists(kStatus)
	tell application "CodeWarrior IDE"
		activate
		try
			open file kProjectFile
			Set Current Target kTarget
			Remove Object Code -- the project's "Remove Object Code before every rebuild" rule
			try
				set errs to Make Project with ExternalEditor
				Save Error Window As kErrLog
				if (count of errs) is 0 then
					my writeStatus("OK 0")
				else
					my writeStatus("OK " & (count of errs)) -- built, with warnings
				end if
			on error m number n
				Save Error Window As kErrLog
				my writeStatus("FAIL " & n & " " & m) -- n=5 => errShell_MakeFailed
			end try
		on error m number n
			my writeStatus("ABORT " & n & " " & m) -- project open / target failure
		end try
	end tell
	-- delete the trigger so we don't rebuild in a loop, then signal completion
	rmIfExists(kTrigger)
	touch(kDone)
end doBuild

on writeStatus(s)
	try
		set fp to open for access file kStatus with write permission
		set eof fp to 0
		write s to fp
		close access fp
	on error
		try
			close access file kStatus
		end try
	end try
end writeStatus

on touch(p)
	try
		set fp to open for access file p with write permission
		set eof fp to 0
		write "done" to fp
		close access fp
	on error
		try
			close access file p
		end try
	end try
end touch

on fileExists(p)
	tell application "Finder"
		return exists file p
	end tell
end fileExists

on rmIfExists(p)
	tell application "Finder"
		if exists file p then delete file p
	end tell
end rmIfExists
