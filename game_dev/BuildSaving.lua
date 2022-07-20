-- BuildSaving.lua
-- Module that allows for (F3X) build saving

--------------------------

local Workspace = game:GetService("Workspace")
local DataStoreService = game:GetService("DataStoreService")
local Players = game:GetService("Players")
local HttpService = game:GetService("HttpService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local CollectionService = game:GetService("CollectionService")

local ServerModules = script.Parent
local InstanceSerializer = require(ServerModules:WaitForChild("InstanceSerializer"))
local RBXUtility = require(ServerModules:WaitForChild("RBXUtility"))
local BuildLocking = require(ServerModules:WaitForChild("BuildLocking"))
local BackupSaving = require(ServerModules:WaitForChild("BackupSaving"))
local ImageFilter = require(ServerModules:WaitForChild("ImageFilter"))
local Network = game:GetService("ReplicatedStorage"):WaitForChild("Network")
local BuildSavingRemote = Network:WaitForChild("BuildSaving")
local BuildInfoDataStore = DataStoreService:GetDataStore("BuildInfoDataStore")

local AHPartAntiLag
coroutine.wrap(function()
	while not _G.AHPartAntiLag do RBXUtility.Wait() end
	AHPartAntiLag = _G.AHPartAntiLag -- not ideal... but circular dependency :/
end)()
-- actual build data is stored in a player-specific datastore: DataStoreService:GetDataStore("BuildDataDataStore", SCOPE)

-- note: build DATA not info
local BuildDataRetryLimit = 4

local FailedRetryTime = 0.5
local DataCutoff = 260000	-- update in AHPartANtiLag
local KeyNameLimit = 47	-- 50 is the true max, but we need 1 for \127, 2 for 1-99 (markers for chunk)


--------------------------

-- player_id = [true]
local RequestsMutex = {}
local PlayerBuildInfo = {}
--[[
	-- need this so we can display it to the user
	key = [table]
	userID = {
		[buildname] = data store slots (1,2,3,...),
		[buildname] = 
	}
	
]]--
local PlayerBuildData = {}	-- also serves as a cache
--[[
	userID[SCOPE] = {
		key = ...
		["magic"] = SERIALIZED_DATA
		["magic2"] = .....
		["house"] = SERIALIZED_DATA
	}	
]]--

--------------------------

-- Has a TIMEOUT
local function GetBuildData(PlayerID, Key)
	local BuildDataDataStore = DataStoreService:GetDataStore("BuildDataDataStore", PlayerID)
	local Success, Result = RBXUtility.RetryWithLimit(BuildDataDataStore.GetAsync, false, "", FailedRetryTime, BuildDataRetryLimit, BuildDataDataStore, Key)

	return Success and Result or error("Error getting build data. Please retry at a later time. Error: " .. tostring(Result))
end

-- This retries indefinitely
local function SaveBuildData(PlayerID, Key, Data)
	local BuildDataDataStore = DataStoreService:GetDataStore("BuildDataDataStore", PlayerID)
	print("[SaveBuildData] Key:", Key)

	RBXUtility.RetryUntilSuccess(BuildDataDataStore.SetAsync, true, "[BuildSaving::SaveBuildData] Error: ", FailedRetryTime, BuildDataDataStore, Key, Data)
end


--- Build info

local function SaveToBuildInfoKey(Key, Data, Callback)
	RBXUtility.RetryUntilSuccess(BuildInfoDataStore.SetAsync, true, "[BuildSaving::SaveBuildInfo] Error: ", FailedRetryTime, BuildInfoDataStore, Key, Data)
	if Callback then Callback() end
end

local BuildInfoSaveQueue = 0
-- Also retries indefinitely
local function SaveBuildInfo(Key, Wait)
	print("[SaveBuildInfo] Key: " .. Key)
	local Table = PlayerBuildInfo[Key]
	if type(Table) ~= "table" then warn("[BuildSaving::SaveBuildInfo] No data to save. Did the server already save and wipe this key?") return end

	BuildInfoSaveQueue += 1

	local SerializedData = HttpService:JSONEncode(Table)

	local ChunkSize = math.ceil(#SerializedData / DataCutoff)
	local SavedChunks = 0

	for Chunk = 1, ChunkSize do
		local SubKey = Key .. (Chunk == 1 and "" or ("_" .. Chunk))
		local ChunkData
		if Chunk == ChunkSize then -- Last chunk
			ChunkData = SerializedData:sub((Chunk - 1) * DataCutoff + 1, #SerializedData)
		else
			ChunkData = SerializedData:sub((Chunk - 1) * DataCutoff + 1, DataCutoff * Chunk)
		end
		coroutine.wrap(SaveToBuildInfoKey)(SubKey, ChunkData, function() SavedChunks = SavedChunks + 1 end)
	end

	if Wait then
		while SavedChunks ~= ChunkSize do
			RBXUtility.Wait()
		end
		BuildInfoSaveQueue -= 1
	else
		coroutine.wrap(function()
			while SavedChunks ~= ChunkSize do
				RBXUtility.Wait()
			end
			BuildInfoSaveQueue -= 1
		end)()
	end
end

local function GetFromBuildInfoKey(Key)
	return RBXUtility.RetryUntilSuccess(BuildInfoDataStore.GetAsync, true, "[BuildSaving::GetBuildInfo] Error: ", FailedRetryTime, BuildInfoDataStore, Key)
end

-- This retries indefinitely
local function GetBuildInfo(Key)
	local SerializedData = ""
	local Chunk = 1

	while true do
		local SubKey = Key .. (Chunk == 1 and "" or ("_" .. Chunk))
		local Data = GetFromBuildInfoKey(SubKey)

		if Data == nil then
			break	-- could happen if player has no data in the first place
		else
			if type(Data) == "table" then	-- LEGACY: one table system (safe to just return once)
				return Data
			else
				SerializedData = SerializedData .. Data
				if #Data ~= DataCutoff then
					break	-- don't bother checking next chunk if this chunk hasn't reached the cutoff
				end
				Chunk = Chunk + 1
			end		
		end
	end

	if #SerializedData > 0 then
		return HttpService:JSONDecode(SerializedData)
	else
		return nil
	end	
end

--------------------------

-- TEST; when we want to test stuff lol
--[[
if game:GetService("RunService"):IsStudio() then
	coroutine.wrap(function()
		wait(3)
		print'beginning'
	local TestData = GetBuildInfo(318058156)
	local Name = "God Mode Weld"
	
	-- Confirm data exists
	local ChunkSize = TestData[Name]
	print("ChunkSize: " .. tostring(ChunkSize))
	if (not ChunkSize) or (type(ChunkSize) ~= "number") or (ChunkSize < 1) then return end	

	-- Check cache, if not, load -> FOR EACH CHUNK (separately)
	local SerializedData = ""
	for Chunk = 1, ChunkSize do
		local Key = Name .. (Chunk == 1 and "" or ("\127" .. Chunk))
		-- GET
		local Success, Result = pcall(GetBuildData, 318058156, Key)
		if Success then
			-- the result should never be nil, unless saving failed... uh
			-- set cache
			if Result == nil then warn("[BuildSaving::LoadBuild] Error build corruption! " .. tostring(318058156) .. " | " .. tostring(Name)) return "This build may be corrupted. Please report this to Kaderth." end
			SerializedData = SerializedData .. Result
		else
			-- try again later
			print("failed to get data")
		end
	end

	-- print("ChunkSize: " .. ChunkSize)
	-- print(SerializedData)

	local Objects
	local Success, Result = pcall(InstanceSerializer.Deserialize, SerializedData)
	if Success then
		Objects = Result
	else
		warn("[Deserializer Error]: ", Result, "|", 318058156, Name)
	end
	end)()
end]]


local function PlayerAdded(Player)
	local ID = Player.UserId
	-- we get build data on `load` call; not immediately.
	-- we also don't `set` build data --> each key is a diff build
	if PlayerBuildData[ID] == nil then PlayerBuildData[ID] = {} else print(Player.Name .. " already has BuildData.") end
	if PlayerBuildInfo[ID] == nil then	-- so we don't do duplicates
		local PlayerData = GetBuildInfo(ID)
		if PlayerData == nil then
			warn("[BuildSaving::PlayerAdded] New player: " .. tostring(ID))
			PlayerBuildInfo[ID] = {}
			SaveBuildInfo(ID)
		else
			warn("[BuildSaving::PlayerAdded] Old player: " .. tostring(ID))
			PlayerBuildInfo[ID] = PlayerData

			-- Legacy...
			if PlayerData.SessionIDKeyName and tonumber(PlayerData.SessionIDKeyName) == nil then
				PlayerData.SessionIDKeyName = nil
			end
			PlayerData["_\127_Session"] = nil

			-- Should we convert old backups? For now, we'll just erase them ripp
			PlayerData["_\127__TMKEY"] = nil
			PlayerData["_\127_AutoSave"] = nil
		end
	else
		print(Player.Name .. " already has BuildInfo.")
	end
end
RBXUtility.ConnectAndApply(Players.PlayerAdded, PlayerAdded, Players:GetPlayers())

Players.PlayerRemoving:Connect(function(Player)
	local ID = Player.UserId

	-- build DATA is saved right away, and does NOT rely on the table (so setting to nil is fine)
	PlayerBuildData[ID] = nil
	RequestsMutex[ID] = nil

	-- build info
	SaveBuildInfo(ID)
	PlayerBuildInfo[ID] = nil


	warn("[BuildSaving] Player `" .. ID .. "` left. Cleared cache for player & finished sending request to save.")
end)

game:BindToClose(function()	
	-- Theoretically, we should just wait until BuildInfoSaveQueue == 0 since playerremoving will fire, but just in case, we'll do this (no downside/data issues since we already clear our cache and do a cache check)
	local SuccessCount, DesiredSuccessCount = 0, 0
	for ID, Cache in next, PlayerBuildInfo do
		DesiredSuccessCount = DesiredSuccessCount + 1
		coroutine.wrap(function()
			SaveBuildInfo(ID, true)
			SuccessCount = SuccessCount + 1
		end)()
	end

	while SuccessCount ~= DesiredSuccessCount do
		RBXUtility.Wait()
	end
	warn("[BuildSaving] - SHUTDOWN: BINDTOCLOSE SAVING DONE.")

	while BuildInfoSaveQueue ~= 0 do
		RBXUtility.Wait()
	end
	warn("[BuildSaving] - SHUTDOWN: SAVE QUEUE IS EMPTY.")
end)

--------------------------

local BuildSaving = {}

-- mainly for outside scripts
BuildSaving.DoesBuildExist = function(PlayerID, Name)
	local BuildInfo = PlayerBuildInfo[PlayerID]
	if BuildInfo == nil then return false end

	if type(Name) ~= "string" or Name == "" then return false end

	local Info = BuildInfo[Name]
	return (type(Info) == "number" and Info > 0)
end

-- by delete, i mean we'll delete the name of the build from the build INFO
-- the data will remain there (no need to waste set requests on setting data to nil)
-- we'll also delete it from the build data cache
-- to avoid accessing this data again, we must ALWAYS check buildinfo and see if chunksize exists and > 0
BuildSaving.DeleteBuild = function(PlayerID, Name)
	local BuildInfo = PlayerBuildInfo[PlayerID]
	if BuildInfo == nil then return end

	local BuildData = PlayerBuildData[PlayerID]
	if BuildData == nil then return end

	if type(Name) ~= "string" or Name == "" then return end

	local ChunkSize = BuildInfo[Name]
	if (not ChunkSize) or (type(ChunkSize) ~= "number") or (ChunkSize < 1) then return end

	for Chunk = 1, ChunkSize do
		local Key = Name .. (Chunk == 1 and "" or ("\127" .. Chunk))
		BuildData[Key] = nil
	end

	BuildInfo[Name] = nil
	coroutine.wrap(SaveBuildInfo)(PlayerID)
	return true
end

BuildSaving.SaveBuild = function(PlayerID, Name, Objects)
	local BuildInfo = PlayerBuildInfo[PlayerID]
	if BuildInfo == nil then return end

	local BuildData = PlayerBuildData[PlayerID]
	if BuildData == nil then return end

	if type(Name) ~= "string" or Name == "" then return end
	if type(Objects) ~= "table" or #Objects < 1 then return end

	-- Make sure name fits limit
	if #Name > KeyNameLimit then	-- this check should also be on client/server (custom commands)
		return "Slot name is too long. Please choose a shorter name."
	end

	-- any changes below may have to be reflected in backup saving (AHPartAntiLag.lua)
	local SerializedData
	local Success, Result = pcall(InstanceSerializer.Serialize, Objects, {}, true)
	if Success then
		SerializedData = Result
	else
		warn("[Serializer Error]: ", Result)
		return "Unexpected error. Error: " .. Result		-- umm not sure what to do here.. this should never happen?
	end

	local ChunkSize = math.ceil(#SerializedData / DataCutoff)

	BuildInfo[Name] = ChunkSize	-- set chunksize
	coroutine.wrap(SaveBuildInfo)(PlayerID)

	for Chunk = 1, ChunkSize do
		local Key = Name .. (Chunk == 1 and "" or ("\127" .. Chunk))
		local ChunkData
		if Chunk == ChunkSize then	-- last chunk
			ChunkData = SerializedData:sub((Chunk - 1) * DataCutoff + 1, #SerializedData)
		else
			ChunkData = SerializedData:sub((Chunk - 1) * DataCutoff + 1, DataCutoff * Chunk)
		end
		BuildData[Key] = ChunkData	-- set builddata
		coroutine.wrap(SaveBuildData)(PlayerID, Key, ChunkData)
	end

	return true	
end

-- Copy of LoadBuild... basically
BuildSaving.LoadBackupBuild = function(PlayerID, BackupID)
	local Success, ChunkSize = pcall(BackupSaving.GetBackupInfo, PlayerID, BackupID)
	if Success then
		if type(ChunkSize) == "number" and ChunkSize > 0 then
			local SerializedData = ""
			for Chunk = 1, ChunkSize do
				local Success2, ChunkData = pcall(BackupSaving.GetBackupData, PlayerID, BackupID, Chunk)
				if Success2 then
					if type(ChunkData) == "string" then
						SerializedData = SerializedData .. ChunkData
					else
						return "Your backup data hasn't fully saved yet. Try waiting a bit for it to finish saving."
					end
				else
					return "Could not load backup build due to network issues. Please try again later."
				end
			end

			-- Copied straight from below
			local Objects
			local Success, Result = pcall(InstanceSerializer.Deserialize, SerializedData)
			if Success then
				Objects = Result
			else
				warn("[Deserializer BACKUP Error]: ", Result, "|", PlayerID, BackupID)
				return "This build may have been corrupted. This has been reported. Error: " .. Result		-- umm not sure what to do here.. this should never happen?
			end

			local Result = AHPartAntiLag.Register(Objects[1], Players:GetPlayerByUserId(PlayerID), true)
			if Result == false then	-- denied
				Objects[1]:Destroy()
				return "The build you are trying to load exceeds the maximum number of parts (see :partcount for the maximum)."
			end

			local RealObjects = {}

			local Model = Objects[1]	-- it's in a model
			Model.Parent = Workspace

			local function PostInsertionCheck(Object)
				if Object:IsA("VehicleSeat") then	-- weird edge case w/ vehicle seats...
					Object.Disabled = true
					Object.Disabled = false
				end
				for Index, Child in next, Object:GetChildren() do
					PostInsertionCheck(Child)
				end
			end

			-- Tag ALL objects (for unloadb functionality)
			for _, Descendant in next, Model:GetDescendants() do
				CollectionService:AddTag(Descendant, "BUILD_" .. PlayerID)
			end

			-- unpack the model
			for Index, Child in next, Model:GetChildren() do
				ImageFilter.FilterObject(Child)
				Child.Parent = Workspace
				PostInsertionCheck(Child)
				RealObjects[#RealObjects + 1] = Child
			end
			Model:Destroy()

			return true, RealObjects	-- success; return objects for f3x selection highlighting
		else
			return "Your backup does not exist. Try rejoining to refresh your backups."
		end
	else
		return "Could not load backup build due to network issues. Please try again later."
	end	
end

-- name checks are done separately (command plugin is responsible for this)
BuildSaving.LoadBuild = function(PlayerID, Name)
	local BuildInfo = PlayerBuildInfo[PlayerID]
	if BuildInfo == nil then return end

	local BuildData = PlayerBuildData[PlayerID]
	if BuildData == nil then return end

	if type(Name) ~= "string" or Name == "" then return end

	-- Confirm data exists
	local ChunkSize = BuildInfo[Name]
	if (not ChunkSize) or (type(ChunkSize) ~= "number") or (ChunkSize < 1) then return end	

	-- Check cache, if not, load -> FOR EACH CHUNK (separately)
	local SerializedData = ""
	for Chunk = 1, ChunkSize do
		local Key = Name .. (Chunk == 1 and "" or ("\127" .. Chunk))
		if BuildData[Key] then
			SerializedData = SerializedData .. BuildData[Key]
		else
			-- GET
			local Success, Result = pcall(GetBuildData, PlayerID, Key)
			if Success then
				-- the result should never be nil, unless saving failed... uh
				-- set cache
				if Result == nil then warn("[BuildSaving::LoadBuild] Error build corruption! " .. tostring(PlayerID) .. " | " .. tostring(Name)) return "This build may be corrupted. Please report this to Kaderth." end
				BuildData[Key] = Result
				SerializedData = SerializedData .. Result
			else
				-- try again later
				return Result	-- show this message to the player
			end
		end
	end

	-- print("ChunkSize: " .. ChunkSize)

	local Objects
	local Success, Result = pcall(InstanceSerializer.Deserialize, SerializedData)
	if Success then
		Objects = Result
	else
		warn("[Deserializer Error]: ", Result, "|", PlayerID, Name)
		return "This build may have been corrupted. This has been reported. Error: " .. Result		-- umm not sure what to do here.. this should never happen?
	end

	local Result = AHPartAntiLag.Register(Objects[1], Players:GetPlayerByUserId(PlayerID), true)
	if Result == false then	-- denied
		Objects[1]:Destroy()
		return "The build you are trying to load exceeds the maximum number of parts (see :partcount for the maximum)."
	end

	local RealObjects = {}

	local Model = Objects[1]	-- it's in a model
	Model.Parent = Workspace

	local function PostInsertionCheck(Object)
		if Object:IsA("VehicleSeat") then	-- weird edge case w/ vehicle seats...
			Object.Disabled = true
			Object.Disabled = false
		end
		for Index, Child in next, Object:GetChildren() do
			PostInsertionCheck(Child)
		end
	end

	-- Tag ALL objects (for unloadb functionality)
	for _, Descendant in next, Model:GetDescendants() do
		CollectionService:AddTag(Descendant, "BUILD_" .. PlayerID .. "|" .. Name)
		CollectionService:AddTag(Descendant, "BUILD_" .. PlayerID)
	end

	-- unpack the model
	for Index, Child in next, Model:GetChildren() do
		ImageFilter.FilterObject(Child)
		Child.Parent = Workspace
		PostInsertionCheck(Child)
		RealObjects[#RealObjects + 1] = Child
	end
	Model:Destroy()

	return true, RealObjects	-- success; return objects for f3x selection highlighting
end

BuildSaving.IsBusy = function(PlayerID)
	return RequestsMutex[PlayerID] == true
end

BuildSaving.SetBusy = function(PlayerID, Value)
	RequestsMutex[PlayerID] = Value
end

-- Not encrypting atm
BuildSavingRemote.OnServerInvoke = function(Player, Data)
	-- Check if PlayerBuildInfo loaded in
	local BuildInfo = PlayerBuildInfo[Player.UserId]
	if BuildInfo == nil then return false end

	-- "GET" / "ISBUSY" will not go through the debounce
	if type(Data) ~= "table" then return false end
	if Data[1] == "GET" then
		local CopyOfBuildInfo = {}
		for Name, Size in next, BuildInfo do
			CopyOfBuildInfo[Name] = Size
		end
		local BackupID = BackupSaving.GetCurrentBackupID(Player.UserId)
		local TimeStamp = nil
		if BackupID ~= 0 then
			print("Current backup ID:", BackupID)
			local Success, _, TempTS = pcall(BackupSaving.GetBackupInfo, Player.UserId, BackupID)
			if Success then TimeStamp = TempTS end
		end
		return CopyOfBuildInfo, BackupID, TimeStamp
	elseif Data[1] == "ISBUSY" then
		return BuildSaving.IsBusy(Player.UserId)
	end

	-- Check debounce
	if RequestsMutex[Player.UserId] then warn("[BuildSavingRemote.OnServerInvoke] Request from " .. Player.Name .. " rejected. Busy.") return -999 end	-- -999 will represent "busy"
	RequestsMutex[Player.UserId] = true	-- can only call this remote one at a time

	local ReturnValue = false

	-- Argument check
	local PacketType = Data[1]
	if PacketType and type(PacketType) == "string" then
		print("Received data from " .. Player.Name .. ". ", unpack(Data))
		if PacketType == "LOAD" then
			local Name = Data[2]
			if Name and type(Name) == "string" then
				local ChunkSize = BuildInfo[Name]
				if ChunkSize and type(ChunkSize) == "number" and ChunkSize > 0 then
					local Result, Objects = BuildSaving.LoadBuild(Player.UserId, Name)
					if type(Result) == "string" then
						ReturnValue = {["Error"] = true, ["Message"] = Result}
					elseif Result == true then
						ReturnValue = {["Success"] = true, ["Objects"] = Objects}
					else
						ReturnValue = {["Error"] = true, ["Message"] = "An unexpected error occurred. Please try again later."}
					end
				end
			end
		elseif PacketType == "LOADBACKUP" then
			local BackupID = Data[2]
			if BackupID and type(BackupID) == "number" then
				local Result, Objects = BuildSaving.LoadBackupBuild(Player.UserId, BackupID)
				if type(Result) == "string" then
					ReturnValue = {["Error"] = true, ["Message"] = Result}
				elseif Result == true then
					ReturnValue = {["Success"] = true, ["Objects"] = Objects}
				else
					ReturnValue = {["Error"] = true, ["Message"] = "An unexpected error occurred. Please try again later."}
				end
			end
		elseif PacketType == "SAVE" then
			local Name, Objects = Data[2], Data[3]
			if Name and Objects and type(Name) == "string" and type(Objects) == "table"  and #Objects > 0 then
				-- We will do filtering here ONLY (direct calls don't need filtering)
				local FilteredList, RejectedPartCount, RejectedNames = BuildLocking.FilterPartList(Player.UserId, Objects)
				if #FilteredList == 0 then
					ReturnValue = {
						["Success"] = true,
						["CompleteFailure"] = true,
						["Message"] = string.format(RejectedPartCount .. " %s could not be saved because %s %s (%s) %s their builds locked/you are not on their %s.",
							RejectedPartCount == 1 and "part" or "parts",
							RejectedPartCount == 1 and "its" or "their",
							#RejectedNames == 1 and "owner" or "owners",
							#RejectedNames > 0 and table.concat(RejectedNames, ", ") or "[owners have left]",
							RejectedPartCount == 1 and "has" or "have",
							#RejectedNames == 1 and "whitelist" or "whitelists"
						)
					}
				else
					local Result = BuildSaving.SaveBuild(Player.UserId, Name, FilteredList)
					if type(Result) == "string" then
						ReturnValue = {["Error"] = true, ["Message"] = Result}
					elseif Result == true then
						if RejectedPartCount == 0 then
							ReturnValue = true	-- legacy
						else
							ReturnValue = {
								["Success"] = true,
								["Message"] = string.format(RejectedPartCount .. " %s could not be saved because %s %s (%s) %s their builds locked/you are not on their %s.",
									RejectedPartCount == 1 and "part" or "parts",
									RejectedPartCount == 1 and "its" or "their",
									#RejectedNames == 1 and "owner" or "owners",
									#RejectedNames > 0 and table.concat(RejectedNames, ", ") or "[owners have left]",
									RejectedPartCount == 1 and "has" or "have",
									#RejectedNames == 1 and "whitelist" or "whitelists"
								)
							}
						end
					else
						ReturnValue = {["Error"] = true, ["Message"] = "An unexpected error occurred. Please try again later."}
					end
				end
			end
		elseif PacketType == "DEL" then
			local Name = Data[2]
			if Name and type(Name) == "string" then
				-- checks will be done in function
				ReturnValue = BuildSaving.DeleteBuild(Player.UserId, Name) or false
			end
		end
	end

	RequestsMutex[Player.UserId] = nil
	return ReturnValue
end

return BuildSaving