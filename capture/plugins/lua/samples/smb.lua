SMB2_CMD = {
  [0x00] = {
    ["name"] = "NegotiateProtocol"
  },
  [0x01] = {
    ["name"] = "SessionSetup"
  },
  [0x02] = {
    ["name"] = "SessionLogoff"
  },
  [0x03] = {
    ["name"] = "TreeConnect"
  },
  [0x04] = {
    ["name"] = "TreeDisconnect"
  },
  [0x05] = {
    ["name"] = "Create"
  },
  [0x06] = {
    ["name"] = "Close"
  },
  [0x07] = {
    ["name"] = "Flush"
  },
  [0x08] = {
    ["name"] = "Read"
  },
  [0x09] = {
    ["name"] = "Write"
  },
  [0x0a] = {
    ["name"] = "Lock"
  },
  [0x0b] = {
    ["name"] = "Ioctl",
    ["cmdpos"] = 73,
    ["cmdlen"] = 4,
    ["00060194"] = "FSCTL_DFS_GET_REFERRALS",
    ["000601B0"] = "FSCTL_DFS_GET_REFERRALS_EX",
    ["00090000"] = "FSCTL_REQUEST_OPLOCK_LEVEL_1",
    ["00090004"] = "FSCTL_REQUEST_OPLOCK_LEVEL_2",
    ["00090008"] = "FSCTL_REQUEST_BATCH_OPLOCK",
    ["0009000C"] = "FSCTL_OPLOCK_BREAK_ACKNOWLEDGE",
    ["00090010"] = "FSCTL_OPBATCH_ACK_CLOSE_PENDING",
    ["00090014"] = "FSCTL_OPLOCK_BREAK_NOTIFY",
    ["00090018"] = "FSCTL_LOCK_VOLUME",
    ["0009001C"] = "FSCTL_UNLOCK_VOLUME",
    ["00090020"] = "FSCTL_DISMOUNT_VOLUME",
    ["00090028"] = "FSCTL_IS_VOLUME_MOUNTED",
    ["0009002C"] = "FSCTL_IS_PATHNAME_VALID",
    ["00090030"] = "FSCTL_MARK_VOLUME_DIRTY",
    ["0009003B"] = "FSCTL_QUERY_RETRIEVAL_POINTERS",
    ["0009003C"] = "FSCTL_GET_COMPRESSION",
    ["0009004F"] = "FSCTL_MARK_AS_SYSTEM_HIVE",
    ["00090050"] = "FSCTL_OPLOCK_BREAK_ACK_NO_2",
    ["00090054"] = "FSCTL_INVALIDATE_VOLUMES",
    ["00090058"] = "FSCTL_QUERY_FAT_BPB",
    ["0009005C"] = "FSCTL_REQUEST_FILTER_OPLOCK",
    ["00090060"] = "FSCTL_FILESYSTEM_GET_STATISTICS",
    ["00090064"] = "FSCTL_GET_NTFS_VOLUME_DATA",
    ["00090068"] = "FSCTL_GET_NTFS_FILE_RECORD",
    ["0009006F"] = "FSCTL_GET_VOLUME_BITMAP",
    ["00090073"] = "FSCTL_GET_RETRIEVAL_POINTERS",
    ["00090074"] = "FSCTL_MOVE_FILE",
    ["00090078"] = "FSCTL_IS_VOLUME_DIRTY",
    ["0009007C"] = "FSCTL_GET_HFS_INFORMATION",
    ["00090083"] = "FSCTL_ALLOW_EXTENDED_DASD_IO",
    ["00090087"] = "FSCTL_READ_PROPERTY_DATA",
    ["0009008B"] = "FSCTL_WRITE_PROPERTY_DATA",
    ["0009008F"] = "FSCTL_FIND_FILES_BY_SID",
    ["00090097"] = "FSCTL_DUMP_PROPERTY_DATA",
    ["0009009C"] = "FSCTL_GET_OBJECT_ID",
    ["000900A4"] = "FSCTL_SET_REPARSE_POINT",
    ["000900A8"] = "FSCTL_GET_REPARSE_POINT",
    ["000900C0"] = "FSCTL_CREATE_OR_GET_OBJECT_ID",
    ["000900C4"] = "FSCTL_SET_SPARSE",
    ["000900D4"] = "FSCTL_SET_ENCRYPTION",
    ["000900DB"] = "FSCTL_ENCRYPTION_FSCTL_IO",
    ["000900DF"] = "FSCTL_WRITE_RAW_ENCRYPTED",
    ["000900E3"] = "FSCTL_READ_RAW_ENCRYPTED",
    ["000900F0"] = "FSCTL_EXTEND_VOLUME",
    ["00090244"] = "FSCTL_CSV_TUNNEL_REQUEST",
    ["0009027C"] = "FSCTL_GET_INTEGRITY_INFORMATION",
    ["00090284"] = "FSCTL_QUERY_FILE_REGIONS",
    ["000902c8"] = "FSCTL_CSV_SYNC_TUNNEL_REQUEST",
    ["00090300"] = "FSCTL_QUERY_SHARED_VIRTUAL_DISK_SUPPORT",
    ["00090304"] = "FSCTL_SVHDX_SYNC_TUNNEL_REQUEST",
    ["00090308"] = "FSCTL_SVHDX_SET_INITIATOR_INFORMATION",
    ["0009030C"] = "FSCTL_SET_EXTERNAL_BACKING",
    ["00090310"] = "FSCTL_GET_EXTERNAL_BACKING",
    ["00090314"] = "FSCTL_DELETE_EXTERNAL_BACKING",
    ["00090318"] = "FSCTL_ENUM_EXTERNAL_BACKING",
    ["0009031F"] = "FSCTL_ENUM_OVERLAY",
    ["00090350"] = "FSCTL_STORAGE_QOS_CONTROL",
    ["00090364"] = "FSCTL_SVHDX_ASYNC_TUNNEL_REQUEST",
    ["000940B3"] = "FSCTL_ENUM_USN_DATA",
    ["000940B7"] = "FSCTL_SECURITY_ID_CHECK",
    ["000940BB"] = "FSCTL_READ_USN_JOURNAL",
    ["000940CF"] = "FSCTL_QUERY_ALLOCATED_RANGES",
    ["000940E7"] = "FSCTL_CREATE_USN_JOURNAL",
    ["000940EB"] = "FSCTL_READ_FILE_USN_DATA",
    ["000940EF"] = "FSCTL_WRITE_USN_CLOSE_RECORD",
    ["00094264"] = "FSCTL_OFFLOAD_READ",
    ["00098098"] = "FSCTL_SET_OBJECT_ID",
    ["000980A0"] = "FSCTL_DELETE_OBJECT_ID",
    ["000980A4"] = "FSCTL_SET_REPARSE_POINT",
    ["000980AC"] = "FSCTL_DELETE_REPARSE_POINT",
    ["000980BC"] = "FSCTL_SET_OBJECT_ID_EXTENDED",
    ["000980C8"] = "FSCTL_SET_ZERO_DATA",
    ["000980D0"] = "FSCTL_ENABLE_UPGRADE",
    ["00098208"] = "FSCTL_FILE_LEVEL_TRIM",
    ["00098268"] = "FSCTL_OFFLOAD_WRITE",
    ["0009C040"] = "FSCTL_SET_COMPRESSION",
    ["0009C280"] = "FSCTL_SET_INTEGRITY_INFORMATION",
    ["00110018"] = "FSCTL_PIPE_WAIT",
    ["0011400C"] = "FSCTL_PIPE_PEEK",
    ["0011C017"] = "FSCTL_PIPE_TRANSCEIVE",
    ["00140078"] = "FSCTL_SRV_REQUEST_RESUME_KEY",
    ["001401D4"] = "FSCTL_LMR_REQUEST_RESILIENCY",
    ["001401FC"] = "FSCTL_QUERY_NETWORK_INTERFACE_INFO",
    ["00140200"] = "FSCTL_VALIDATE_NEGOTIATE_INFO_224",
    ["00140204"] = "FSCTL_VALIDATE_NEGOTIATE_INFO",
    ["00144064"] = "FSCTL_SRV_ENUMERATE_SNAPSHOTS",
    ["001440F2"] = "FSCTL_SRV_COPYCHUNK",
    ["001441bb"] = "FSCTL_SRV_READ_HASH",
    ["001480F2"] = "FSCTL_SRV_COPYCHUNK_WRITE"
  },
  [0x0c] = {
    ["name"] = "Cancel"
  },
  [0x0d] = {
    ["name"] = "KeepAlive"
  },
  [0x0e] = {
    ["name"] = "Find",
    ["cmdpos"] = 71,
    ["cmdlen"] = 1,
    [0x01] = "SMB2_FIND_DIRECTORY_INFO",
    [0x02] = "SMB2_FIND_FULL_DIRECTORY_INFO",
    [0x03] = "SMB2_FIND_BOTH_DIRECTORY_INFO",
    [0x04] = "SMB2_FIND_INDEX_SPECIFIED",
    [0x0C] = "SMB2_FIND_NAME_INFO",
    [0x25] = "SMB2_FIND_ID_BOTH_DIRECTORY_INFO",
    [0x26] = "SMB2_FIND_ID_FULL_DIRECTORY_INFO"
  },
  [0x0f] = {
    ["name"] = "Notify"
  },
  [0x10] = {
    ["name"] = "GetInfo"
  },
  [0x11] = {
    ["name"] = "SetInfo"
  },
  [0x12] = {
    ["name"] = "Break"
  }
}
SMB_SHARE_TYPE = {
  [0x01] = "Physical Disk",
  [0x02] = "Named Pipe",
  [0x03] = "Printer"
}
function parseSMB2(session, str, direction)
  if str:len() < 22 then
    return 0
  end
  if str:byte(21) % 2 == 0 then
    if str:byte(17) == string.char(0x00) then
      return 0
    end
    local tbl = session:table()
    if SMB2_CMD[str:byte(17)] ~= nil and not contains(tbl.opcodes,SMB2_CMD[str:byte(17)]["name"]) then
      print("Added smb opcode: " .. SMB2_CMD[str:byte(17)]["name"])
      table.insert(tbl.opcodes,SMB2_CMD[str:byte(17)]["name"])
      session:add_string("smb.opcode",SMB2_CMD[str:byte(17)]["name"])
      if SMB2_CMD[str:byte(17)]["cmdpos"] == nil or
         SMB2_CMD[str:byte(17)]["cmdpos"] == nil then
         return 0
      end
      if str:len() < SMB2_CMD[str:byte(17)]["cmdpos"] + SMB2_CMD[str:byte(17)]["cmdlen"] - 1 then
        return 0
      end
      local beginPos = SMB2_CMD[str:byte(17)]["cmdpos"]
      local endPos = SMB2_CMD[str:byte(17)]["cmdpos"] + SMB2_CMD[str:byte(17)]["cmdlen"] - 1
      local pattern = ""
      for i = 1, SMB2_CMD[str:byte(17)]["cmdlen"], 1 do
        pattern = pattern .. "%02X"
      end
      local ctx = string.format(pattern,str:sub(beginPos,endPos):reverse():byte(1,SMB2_CMD[str:byte(17)]["cmdlen"]))
      if SMB2_CMD[str:byte(17)][ctx] ~= nil and not contains(tbl.cmd,SMB2_CMD[str:byte(17)][ctx]) then
        print("Added smb cmd: " .. SMB2_CMD[str:byte(17)][ctx])
        table.insert(tbl.cmd,SMB2_CMD[str:byte(17)][ctx])
        session:add_string("smb.cmd", SMB2_CMD[str:byte(17)][ctx])
      end
    end
  end
  return 0
end

function parseSMB1(session, str, direction)
end

function smbClassify(session, str, direction)
  if str:len() >= 64 then
    local tbl = session:table()
    if str:sub(5,8) == string.char(0xff, 0x53, 0x4d, 0x42) then
      if tbl['ver'] == nil then
        tbl.opcodes = {} 
        tbl.cmd = {}
        session:add_protocol("smb")
        tbl.ver = 'SMB1'
        session:add_string("smb.ver", "SMB1")
        print("Added smb parser")
        session:register_parser(parseSMB1)
      end
      return 0
    end
    if str:sub(5,8) == string.char(0xfe, 0x53, 0x4d, 0x42) then
      if tbl['ver'] == nil then
        tbl.opcodes = {} 
        tbl.cmd = {}
        session:add_protocol("smb")
        tbl.ver = 'SMB2'
        session:add_string("smb.ver", "SMB2")
        print("Added smb parser")
        session:register_parser(parseSMB2)
      end
      return 0
    end
  end
end

function contains(tab,val)
  for index, value in ipairs(tab) do
    if value == val then
      return true
    end
  end
  return false
end

MolochSession.register_tcp_classifier("smb", 4, string.char(0xff, 0x53, 0x4d, 0x42), "smbClassify")
MolochSession.register_tcp_classifier("smb", 4, string.char(0xfe, 0x53, 0x4d, 0x42), "smbClassify")
