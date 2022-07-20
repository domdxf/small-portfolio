#pragma once

#include <string>

// https://github.com/davidm/lua2c
// lua2c is entirely in lua; we execute this in our (vanilla) lua VM

std::string base_lua = R"###(
----------------------------------------------------------------------
----------------------------------------------------------------------
--
-- Base library extension
--
----------------------------------------------------------------------
----------------------------------------------------------------------

if not metalua then rawset(getfenv(), 'metalua', { }) end
metalua.version             = "v-0.5"

if not rawpairs then
   rawpairs, rawipairs, rawtype = pairs, ipairs, type
end

function pairs(x)
   assert(type(x)=='table', 'pairs() expects a table')
   local mt = getmetatable(x)
   if mt then
      local mtp = mt.__pairs
      if mtp then return mtp(x) end
   end
   return rawpairs(x)
end

function ipairs(x)
   assert(type(x)=='table', 'ipairs() expects a table')
   local mt = getmetatable(x)
   if mt then
      local mti = mt.__ipairs
      if mti then return mti(x) end
   end
   return rawipairs(x)
end

--[[
function type(x)
   local mt = getmetatable(x)
   if mt then
      local mtt = mt.__type
      if mtt then return mtt end
   end
   return rawtype(x)
end
]]

function min (a, ...)
   for n in values{...} do if n<a then a=n end end
   return a
end

function max (a, ...)
   for n in values{...} do if n>a then a=n end end
   return a
end

function o (...)
   local args = {...}
   local function g (...)
      local result = {...}
      for i=#args, 1, -1 do result = {args[i](unpack(result))} end
      return unpack (result)
   end
   return g
end

function id (...) return ... end
function const (k) return function () return k end end

function printf(...) return print(string.format(...)) end
function eprintf(...) 
   io.stderr:write(string.format(...).."\n") 
end

function ivalues (x)
   assert(type(x)=='table', 'ivalues() expects a table')
   local i = 1
   local function iterator ()
      local r = x[i]; i=i+1; return r
   end
   return iterator
end


function values (x)
   assert(type(x)=='table', 'values() expects a table')
   local function iterator (state)
      local it
      state.content, it = next(state.list, state.content)
      return it
   end
   return iterator, { list = x }
end

function keys (x)
   assert(type(x)=='table', 'keys() expects a table')
   local function iterator (state)
      local it = next(state.list, state.content)
      state.content = it
      return it
   end
   return iterator, { list = x }
end
)###";

std::string table2_lua = R"###(
---------------------------------------------------------------------
----------------------------------------------------------------------
--
-- Table module extension
--
----------------------------------------------------------------------
----------------------------------------------------------------------

-- todo: table.scan (scan1?) fold1? flip?

function table.transpose(t)
   local tt = { }
   for a, b in pairs(t) do tt[b] = a end
   return tt
end

function table.iforeach(f, ...)
   -- assert (type (f) == "function") [wouldn't allow metamethod __call]
   local nargs = select("#", ...)
   if nargs==1 then -- Quick iforeach (most common case), just one table arg
      local t = ...
      assert (type (t) == "table")
      for i = 1, #t do 
         local result = f (t[i])
         -- If the function returns non-false, stop iteration
         if result then return result end
      end
   else -- advanced case: boundaries and/or multiple tables
      -- 1 - find boundaries if any
      local  args, fargs, first, last, arg1 = {...}, { }
      if     type(args[1]) ~= "number" then first, arg1 = 1, 1
      elseif type(args[2]) ~= "number" then first, last, arg1 = 1, args[1], 2
      else   first,  last, i = args[1], args[2], 3 end
      assert (nargs > arg1)
      -- 2 - determine upper boundary if not given
      if not last then for i = arg1, nargs do 
            assert (type (args[i]) == "table")
            last = max (#args[i], last) 
      end end
      -- 3 - perform the iteration
      for i = first, last do
         for j = arg1, nargs do fargs[j] = args[j][i] end -- build args list
         local result = f (unpack (fargs)) -- here is the call
         -- If the function returns non-false, stop iteration
         if result then return result end
      end
   end
end

function table.imap (f, ...)
   local result, idx = { }, 1
   local function g(...) result[idx] = f(...);  idx=idx+1 end
   table.iforeach(g, ...)
   return result
end

function table.ifold (f, acc, ...)
   local function g(...) acc = f (acc,...) end
   table.iforeach (g, ...)
   return acc
end

-- function table.ifold1 (f, ...)
--    return table.ifold (f, acc, 2, false, ...)
-- end

function table.izip(...)
   local function g(...) return {...} end
   return table.imap(g, ...)
end

function table.ifilter(f, t)
   local yes, no = { }, { }
   for i=1,#t do table.insert (f(t[i]) and yes or no, t[i]) end
   return yes, no
end

function table.icat(...)
   local result = { }
   for t in values {...} do
      for x in values (t) do
         table.insert (result, x)
      end
   end
   return result
end

function table.iflatten (x) return table.icat (unpack (x)) end

function table.irev (t)
   local result, nt = { }, #t
   for i=0, nt-1 do result[nt-i] = t[i+1] end
   return result
end

function table.isub (t, ...)
   local ti, u = table.insert, { }
   local args, nargs = {...}, select("#", ...)
   for i=1, nargs/2 do
      local a, b = args[2*i-1], args[2*i]
      for i=a, b, a<=b and 1 or -1 do ti(u, t[i]) end
   end
   return u
end

function table.iall (f, ...)
   local result = true
   local function g(...) return not f(...) end
   return not table.iforeach(g, ...)
   --return result
end

function table.iany (f, ...)
   local function g(...) return not f(...) end
   return not table.iall(g, ...)
end

function table.shallow_copy(x)
   local y={ }
   for k, v in pairs(x) do y[k]=v end
   return y
end

-- Warning, this is implementation dependent: it relies on
-- the fact the [next()] enumerates the array-part before the hash-part.
function table.cat(...)
   local y={ }
   for x in values{...} do
      -- cat array-part
      for _, v in ipairs(x) do table.insert(y,v) end
      -- cat hash-part
      local lx, k = #x
      if lx>0 then k=next(x,lx) else k=next(x) end
      while k do y[k]=x[k]; k=next(x,k) end
   end
   return y
end

function table.deep_copy(x) 
   local tracker = { }
   local function aux (x)
      if type(x) == "table" then
         local y=tracker[x]
         if y then return y end
         y = { }; tracker[x] = y
         setmetatable (y, getmetatable (x))
         for k,v in pairs(x) do y[aux(k)] = aux(v) end
         return y
      else return x end
   end
   return aux(x)
end

function table.override(dst, src)
   for k, v in pairs(src) do dst[k] = v end
   for i = #src+1, #dst   do dst[i] = nil end
   return dst
end


function table.range(a,b,c)
   if not b then assert(not(c)); b=a; a=1
   elseif not c then c = (b>=a) and 1 or -1 end
   local result = { }
   for i=a, b, c do table.insert(result, i) end
   return result
end

-- FIXME: new_indent seems to be always nil?!
-- FIXME: accumulator function should be configurable,
-- so that print() doesn't need to bufferize the whole string
-- before starting to print.
function table.tostring(t, ...)
   local PRINT_HASH, HANDLE_TAG, FIX_INDENT, LINE_MAX, INITIAL_INDENT = true, true
   for _, x in ipairs {...} do
      if type(x) == "number" then
         if not LINE_MAX then LINE_MAX = x
         else INITIAL_INDENT = x end
      elseif x=="nohash" then PRINT_HASH = false
      elseif x=="notag"  then HANDLE_TAG = false
      else
         local n = string['match'](x, "^indent%s*(%d*)$")
         if n then FIX_INDENT = tonumber(n) or 3 end
      end
   end
   LINE_MAX       = LINE_MAX or math.huge
   INITIAL_INDENT = INITIAL_INDENT or 1
   
   local current_offset =  0  -- indentation level
   local xlen_cache     = { } -- cached results for xlen()
   local acc_list       = { } -- Generated bits of string
   local function acc(...)    -- Accumulate a bit of string
      local x = table.concat{...}
      current_offset = current_offset + #x
      table.insert(acc_list, x) 
   end
   local function valid_id(x)
      -- FIXME: we should also reject keywords; but the list of
      -- current keywords is not fixed in metalua...
      return type(x) == "string" 
         and string['match'](x, "^[a-zA-Z_][a-zA-Z0-9_]*$")
   end
   
   -- Compute the number of chars it would require to display the table
   -- on a single line. Helps to decide whether some carriage returns are
   -- required. Since the size of each sub-table is required many times,
   -- it's cached in [xlen_cache].
   local xlen_type = { }
   local function xlen(x, nested)
      nested = nested or { }
      if x==nil then return #"nil" end
      --if nested[x] then return #tostring(x) end -- already done in table
      local len = xlen_cache[x]
      if len then return len end
      local f = xlen_type[type(x)]
      if not f then return #tostring(x) end
      len = f (x, nested) 
      xlen_cache[x] = len
      return len
   end

		   -- optim: no need to compute lengths if I'm not going to use them
   -- anyway.
   if LINE_MAX == math.huge then xlen = function() return 0 end end

		   xlen_type["nil"] = function () return 3 end
   function xlen_type.number  (x) return #tostring(x) end
   function xlen_type.boolean (x) return x and 4 or 5 end
   function xlen_type.string  (x) return #string.format("%q",x) end
   function xlen_type.table   (adt, nested)

		      -- Circular references detection
      if nested [adt] then return #tostring(adt) end
      nested [adt] = true

		      local has_tag  = HANDLE_TAG and valid_id(adt.tag)
      local alen     = #adt
      local has_arr  = alen>0
      local has_hash = false
      local x = 0
      
      if PRINT_HASH then
         -- first pass: count hash-part
         for k, v in pairs(adt) do
            if k=="tag" and has_tag then 
               -- this is the tag -> do nothing!
            elseif type(k)=="number" and k<=alen and math.fmod(k,1)==0 then 
               -- array-part pair -> do nothing!
            else
               has_hash = true
               if valid_id(k) then x=x+#k
               else x = x + xlen (k, nested) + 2 end -- count surrounding brackets
               x = x + xlen (v, nested) + 5          -- count " = " and ", "
            end
         end
      end

		      for i = 1, alen do x = x + xlen (adt[i], nested) + 2 end -- count ", "
      
      nested[adt] = false -- No more nested calls

		      if not (has_tag or has_arr or has_hash) then return 3 end
      if has_tag then x=x+#adt.tag+1 end
      if not (has_arr or has_hash) then return x end
      if not has_hash and alen==1 and type(adt[1])~="table" then
         return x-2 -- substract extraneous ", "
      end
      return x+2 -- count "{ " and " }", substract extraneous ", "
   end
   
   -- Recursively print a (sub) table at given indentation level.
   -- [newline] indicates whether newlines should be inserted.
   local function rec (adt, nested, indent)
      if not FIX_INDENT then indent = current_offset end
      local function acc_newline()
         acc ("\n"); acc (string.rep (" ", indent)) 
         current_offset = indent
      end
      local x = { }
      x["nil"] = function() acc "nil" end
      function x.number()   acc (tostring (adt)) end
      --function x.string()   acc (string.format ("%q", adt)) end
      function x.string()   acc ((string.format ("%q", adt):gsub("\\\n", "\\n"))) end
      function x.boolean()  acc (adt and "true" or "false") end
      function x.table()
         if nested[adt] then acc(tostring(adt)); return end
         nested[adt]  = true


		         local has_tag  = HANDLE_TAG and valid_id(adt.tag)
         local alen     = #adt
         local has_arr  = alen>0
         local has_hash = false

		         if has_tag then acc("`"); acc(adt.tag) end

		         -- First pass: handle hash-part
         if PRINT_HASH then
            for k, v in pairs(adt) do
               -- pass if the key belongs to the array-part or is the "tag" field
               if not (k=="tag" and HANDLE_TAG) and 
                  not (type(k)=="number" and k<=alen and math.fmod(k,1)==0) then

		                  -- Is it the first time we parse a hash pair?
                  if not has_hash then 
                     acc "{ "
                     if not FIX_INDENT then indent = current_offset end
                  else acc ", " end

		                  -- Determine whether a newline is required
                  local is_id, expected_len = valid_id(k)
                  if is_id then expected_len = #k + xlen (v, nested) + #" = , "
                  else expected_len = xlen (k, nested) + 
                                      xlen (v, nested) + #"[] = , " end
                  if has_hash and expected_len + current_offset > LINE_MAX
                  then acc_newline() end
                  
                  -- Print the key
                  if is_id then acc(k); acc " = " 
                  else  acc "["; rec (k, nested, indent+(FIX_INDENT or 0)); acc "] = " end

		                  -- Print the value
                  rec (v, nested, indent+(FIX_INDENT or 0))
                  has_hash = true
               end
            end
         end

		         -- Now we know whether there's a hash-part, an array-part, and a tag.
         -- Tag and hash-part are already printed if they're present.
         if not has_tag and not has_hash and not has_arr then acc "{ }"; 
         elseif has_tag and not has_hash and not has_arr then -- nothing, tag already in acc
         else 
            assert (has_hash or has_arr)
            local no_brace = false
            if has_hash and has_arr then acc ", " 
            elseif has_tag and not has_hash and alen==1 and type(adt[1])~="table" then
               -- No brace required; don't print "{", remember not to print "}"
               acc (" "); rec (adt[1], nested, indent+(FIX_INDENT or 0))
               no_brace = true
            elseif not has_hash then
               -- Braces required, but not opened by hash-part handler yet
               acc "{ "
               if not FIX_INDENT then indent = current_offset end
            end

		            -- 2nd pass: array-part
            if not no_brace and has_arr then 
               rec (adt[1], nested, indent+(FIX_INDENT or 0))
               for i=2, alen do 
                  acc ", ";                   
                  if   current_offset + xlen (adt[i], { }) > LINE_MAX
                  then acc_newline() end
                  rec (adt[i], nested, indent+(FIX_INDENT or 0)) 
               end
            end
            if not no_brace then acc " }" end
         end
         nested[adt] = false -- No more nested calls
      end
      local y = x[type(adt)]
      if y then y() else acc(tostring(adt)) end
   end
   --printf("INITIAL_INDENT = %i", INITIAL_INDENT)
   current_offset = INITIAL_INDENT or 0
   rec(t, { }, 0)
   return table.concat (acc_list)
end

function table.print(...) return print(table.tostring(...)) end

return table
)###";

std::string string2_lua = R"###(

----------------------------------------------------------------------
----------------------------------------------------------------------
--
-- String module extension
--
----------------------------------------------------------------------
----------------------------------------------------------------------

-- Courtesy of lua-users.org
function string.split(str, pat)
   local t = {} 
   local fpat = "(.-)" .. pat
   local last_end = 1
   local s, e, cap = string.find(str, fpat, 1)
   while s do
      if s ~= 1 or cap ~= "" then
          table.insert(t,cap)
       end
      last_end = e+1
      s, e, cap = string.find(str, fpat, last_end)
   end
   if last_end <= string.len(str) then
      cap = string.sub(str, last_end)
      table.insert(t, cap)
   end
   return t
end

-- "match" is regularly used as a keyword for pattern matching, 
-- so here is an always available substitute.
string.strmatch = string["match"]

-- change a compiled string into a function
function string.undump(str)
   if str:strmatch '^\027LuaQ' or str:strmatch '^#![^\n]+\n\027LuaQ' then
      local f = (lua_loadstring or loadstring)(str)
      return f
   else
      error "Not a chunk dump"
   end
end

return string
)###";

std::string lexer_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mll.lua,v 1.3 2006/11/15 09:07:50 fab13n Exp $
--
-- Summary: generic Lua-style lexer definition. You need this plus
-- some keyword additions to create the complete Lua lexer,
-- as is done in mlp_lexer.lua.
--
-- TODO: 
--
-- * Make it easy to define new flavors of strings. Replacing the
--   lexer.patterns.long_string regexp by an extensible list, with
--   customizable token tag, would probably be enough. Maybe add:
--   + an index of capture for the regexp, that would specify 
--     which capture holds the content of the string-like token
--   + a token tag
--   + or a string->string transformer function.
--
-- * There are some _G.table to prevent a namespace clash which has
--   now disappered. remove them.
----------------------------------------------------------------------
--
-- Copyright (c) 2006, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------

module ("lexer", package.seeall)

lexer = { alpha={ }, sym={ } }
lexer.__index=lexer

local debugf = function() end
--local debugf=printf

----------------------------------------------------------------------
-- Patterns used by [lexer:extract] to decompose the raw string into
-- correctly tagged tokens.
----------------------------------------------------------------------
lexer.patterns = {
   spaces              = "^[ \r\n\t]*()",
   short_comment       = "^%-%-([^\n]*)()\n",
   final_short_comment = "^%-%-([^\n]*)()$",
   long_comment        = "^%-%-%[(=*)%[\n?(.-)%]%1%]()",
   long_string         = "^%[(=*)%[\n?(.-)%]%1%]()",
   number_mantissa     = { "^%d+%.?%d*()", "^%d*%.%d+()" },
   number_exponant     = "^[eE][%+%-]?%d+()",
   number_hex          = "^0[xX]%x+()",
   word                = "^([%a_][%w_]*)()"
}

----------------------------------------------------------------------
-- unescape a whole string, applying [unesc_digits] and
-- [unesc_letter] as many times as required.
----------------------------------------------------------------------
local function unescape_string (s)

   -- Turn the digits of an escape sequence into the corresponding
   -- character, e.g. [unesc_digits("123") == string.char(123)].
   local function unesc_digits (backslashes, digits)
      if #backslashes%2==0 then
         -- Even number of backslashes, they escape each other, not the digits.
         -- Return them so that unesc_letter() can treaat them
         return backslashes..digits
      else
         -- Remove the odd backslash, which escapes the number sequence.
         -- The rest will be returned and parsed by unesc_letter()
         backslashes = backslashes :sub (1,-2)
      end
      local k, j, i = digits:reverse():byte(1, 3)
      local z = _G.string.byte "0"
      local code = (k or z) + 10*(j or z) + 100*(i or z) - 111*z
      if code > 255 then 
          error ("Illegal escape sequence '\\"..digits.."' in string: ASCII codes must be in [0..255]") 
      end
      return backslashes .. string.char (code)
   end

		   -- Take a letter [x], and returns the character represented by the 
   -- sequence ['\\'..x], e.g. [unesc_letter "n" == "\n"].
   local function unesc_letter(x)
      local t = { 
         a = "\a", b = "\b", f = "\f",
         n = "\n", r = "\r", t = "\t", v = "\v",
         ["\\"] = "\\", ["'"] = "'", ['"'] = '"', ["\n"] = "\n" }
      return t[x] or error([[Unknown escape sequence '\]]..x..[[']])
   end

		   return s
      :gsub ("(\\+)([0-9][0-9]?[0-9]?)", unesc_digits)
      :gsub ("\\(%D)",unesc_letter)
end

lexer.extractors = {
   "skip_whitespaces_and_comments",
   "extract_short_string", "extract_word", "extract_number", 
   "extract_long_string", "extract_symbol" }

		lexer.token_metatable = { 
--         __tostring = function(a) 
--            return string.format ("`%s{'%s'}",a.tag, a[1]) 
--         end 
} 
      
lexer.lineinfo_metatable = { }

----------------------------------------------------------------------
-- Really extract next token fron the raw string 
-- (and update the index).
-- loc: offset of the position just after spaces and comments
-- previous_i: offset in src before extraction began
----------------------------------------------------------------------
function lexer:extract ()
   local previous_i = self.i
   local loc = self.i
   local eof, token

		   -- Put line info, comments and metatable around the tag and content
   -- provided by extractors, thus returning a complete lexer token.
   -- first_line: line # at the beginning of token
   -- first_column_offset: char # of the last '\n' before beginning of token
   -- i: scans from beginning of prefix spaces/comments to end of token.
   local function build_token (tag, content)
      assert (tag and content)
      local i, first_line, first_column_offset, previous_line_length =
         previous_i, self.line, self.column_offset, nil

		      -- update self.line and first_line. i := indexes of '\n' chars
      while true do
         i = self.src :find ("\n", i+1, true)
         if not i or i>self.i then break end -- no more '\n' until end of token
         previous_line_length = i - self.column_offset
         if loc and i <= loc then -- '\n' before beginning of token
            first_column_offset = i
            first_line = first_line+1 
         end
         self.line   = self.line+1 
         self.column_offset = i 
      end

		      -- lineinfo entries: [1]=line, [2]=column, [3]=char, [4]=filename
      local fli = { first_line, loc-first_column_offset, loc, self.src_name }
      local lli = { self.line, self.i-self.column_offset-1, self.i-1, self.src_name }
      --Pluto barfes when the metatable is set:(
      setmetatable(fli, lexer.lineinfo_metatable)
      setmetatable(lli, lexer.lineinfo_metatable)
      local a = { tag = tag, lineinfo = { first=fli, last=lli }, content } 
      if lli[2]==-1 then lli[1], lli[2] = lli[1]-1, previous_line_length-1 end
      if #self.attached_comments > 0 then 
         a.lineinfo.comments = self.attached_comments 
         fli.comments = self.attached_comments
         if self.lineinfo_last then
            self.lineinfo_last.comments = self.attached_comments
         end
      end
      self.attached_comments = { }
      return setmetatable (a, self.token_metatable)
   end --</function build_token>

		   for ext_idx, extractor in ipairs(self.extractors) do
      -- printf("method = %s", method)
      local tag, content = self [extractor] (self)
      -- [loc] is placed just after the leading whitespaces and comments;
      -- for this to work, the whitespace extractor *must be* at index 1.
      if ext_idx==1 then loc = self.i end

		      if tag then 
         --printf("`%s{ %q }\t%i", tag, content, loc);
         return build_token (tag, content) 
      end
   end

		   error "None of the lexer extractors returned anything!"
end   

----------------------------------------------------------------------
-- skip whites and comments
-- FIXME: doesn't take into account:
-- - unterminated long comments
-- - short comments at last line without a final \n
----------------------------------------------------------------------
function lexer:skip_whitespaces_and_comments()
   local table_insert = _G.table.insert
   repeat -- loop as long as a space or comment chunk is found
      local _, j
      local again = false
      local last_comment_content = nil
      -- skip spaces
      self.i = self.src:match (self.patterns.spaces, self.i)
      -- skip a long comment if any
      _, last_comment_content, j = 
         self.src :match (self.patterns.long_comment, self.i)
      if j then 
         table_insert(self.attached_comments, 
                         {last_comment_content, self.i, j, "long"})
         self.i=j; again=true 
      end
      -- skip a short comment if any
      last_comment_content, j = self.src:match (self.patterns.short_comment, self.i)
      if j then
         table_insert(self.attached_comments, 
                         {last_comment_content, self.i, j, "short"})
         self.i=j; again=true 
      end
      if self.i>#self.src then return "Eof", "eof" end
   until not again

		   if self.src:match (self.patterns.final_short_comment, self.i) then 
      return "Eof", "eof" end
   --assert (not self.src:match(self.patterns.short_comment, self.i))
   --assert (not self.src:match(self.patterns.long_comment, self.i))
   -- --assert (not self.src:match(self.patterns.spaces, self.i))
   return
end

----------------------------------------------------------------------
-- extract a '...' or "..." short string
----------------------------------------------------------------------
function lexer:extract_short_string()
   -- [k] is the first unread char, [self.i] points to [k] in [self.src]
   local j, k = self.i, self.src :sub (self.i,self.i)
   if k~="'" and k~='"' then return end
   local i = self.i + 1
   local j = i
   while true do
      -- k = opening char: either simple-quote or double-quote
      -- i = index of beginning-of-string
      -- x = next "interesting" character
      -- j = position after interesting char
      -- y = char just after x
      local x, y
      x, j, y = self.src :match ("([\\\r\n"..k.."])()(.?)", j)
      if x == '\\' then j=j+1  -- don't parse escaped char
      elseif x == k then break -- unescaped end of string
      else -- eof or '\r' or '\n' reached before end of string
         assert (not x or x=="\r" or x=="\n")
         error "Unterminated string"
      end
   end
   self.i = j

		   return "String", unescape_string (self.src:sub (i,j-2))
end

----------------------------------------------------------------------
--
----------------------------------------------------------------------
function lexer:extract_word()
   -- Id / keyword
   local word, j = self.src:match (self.patterns.word, self.i)
   if word then
      self.i = j
      if self.alpha [word] then return "Keyword", word
      else return "Id", word end
   end
end

----------------------------------------------------------------------
--
----------------------------------------------------------------------
function lexer:extract_number()
   -- Number
   local j = self.src:match(self.patterns.number_hex, self.i)
   if not j then
      j = self.src:match (self.patterns.number_mantissa[1], self.i) or
          self.src:match (self.patterns.number_mantissa[2], self.i)
      if j then
         j = self.src:match (self.patterns.number_exponant, j) or j;
      end
   end
   if not j then return end
   -- Number found, interpret with tonumber() and return it
   local n = tonumber (self.src:sub (self.i, j-1))
   self.i = j
   return "Number", n
end

----------------------------------------------------------------------
--
----------------------------------------------------------------------
function lexer:extract_long_string()
   -- Long string
   local _, content, j = self.src:match (self.patterns.long_string, self.i)
   if j then self.i = j; return "String", content end
end

----------------------------------------------------------------------
--
----------------------------------------------------------------------
function lexer:extract_symbol()
   -- compound symbol
   local k = self.src:sub (self.i,self.i)
   local symk = self.sym [k]
   if not symk then 
      self.i = self.i + 1
      return "Keyword", k
   end
   for _, sym in pairs (symk) do
      if sym == self.src:sub (self.i, self.i + #sym - 1) then 
         self.i = self.i + #sym; 
         return "Keyword", sym
      end
   end
   -- single char symbol
   self.i = self.i+1
   return "Keyword", k
end

----------------------------------------------------------------------
-- Add a keyword to the list of keywords recognized by the lexer.
----------------------------------------------------------------------
function lexer:add (w, ...)
   assert(not ..., "lexer:add() takes only one arg, although possibly a table")
   if type (w) == "table" then
      for _, x in ipairs (w) do self:add (x) end
   else
      if w:match (self.patterns.word .. "$") then self.alpha [w] = true
      elseif w:match "^%p%p+$" then 
         local k = w:sub(1,1)
         local list = self.sym [k]
         if not list then list = { }; self.sym [k] = list end
         _G.table.insert (list, w)
      elseif w:match "^%p$" then return
      else error "Invalid keyword" end
   end
end

----------------------------------------------------------------------
-- Return the [n]th next token, without consumming it.
-- [n] defaults to 1. If it goes pass the end of the stream, an EOF
-- token is returned.
----------------------------------------------------------------------
function lexer:peek (n)
   if not n then n=1 end
   if n > #self.peeked then
      for i = #self.peeked+1, n do
         self.peeked [i] = self:extract()
      end
   end
  return self.peeked [n]
end

----------------------------------------------------------------------
-- Return the [n]th next token, removing it as well as the 0..n-1
-- previous tokens. [n] defaults to 1. If it goes pass the end of the
-- stream, an EOF token is returned.
----------------------------------------------------------------------
function lexer:next (n)
   n = n or 1
   self:peek (n)
   local a
   for i=1,n do 
      a = _G.table.remove (self.peeked, 1) 
      if a then 
         --debugf ("lexer:next() ==> %s %s",
         --        table.tostring(a), tostring(a))
      end
      self.lastline = a.lineinfo.last[1]
   end
   self.lineinfo_last = a.lineinfo.last
   return a or eof_token
end

----------------------------------------------------------------------
-- Returns an object which saves the stream's current state.
----------------------------------------------------------------------
-- FIXME there are more fields than that to save
function lexer:save () return { self.i; _G.table.cat(self.peeked) } end

----------------------------------------------------------------------
-- Restore the stream's state, as saved by method [save].
----------------------------------------------------------------------
-- FIXME there are more fields than that to restore
function lexer:restore (s) self.i=s[1]; self.peeked=s[2] end

----------------------------------------------------------------------
-- Resynchronize: cancel any token in self.peeked, by emptying the
-- list and resetting the indexes
----------------------------------------------------------------------
function lexer:sync()
   local p1 = self.peeked[1]
   if p1 then 
      li = p1.lineinfo.first
      self.line, self.i = li[1], li[3]
      self.column_offset = self.i - li[2]
      self.peeked = { }
      self.attached_comments = p1.lineinfo.first.comments or { }
   end
end

----------------------------------------------------------------------
-- Take the source and offset of an old lexer.
----------------------------------------------------------------------
function lexer:takeover(old)
   self:sync()
   self.line, self.column_offset, self.i, self.src, self.attached_comments =
      old.line, old.column_offset, old.i, old.src, old.attached_comments
   return self
end

-- function lexer:lineinfo()
-- 	if self.peeked[1] then return self.peeked[1].lineinfo.first
--     else return { self.line, self.i-self.column_offset, self.i } end
-- end


----------------------------------------------------------------------
-- Return the current position in the sources. This position is between
-- two tokens, and can be within a space / comment area, and therefore
-- have a non-null width. :lineinfo_left() returns the beginning of the
-- separation area, :lineinfo_right() returns the end of that area.
--
--    ____ last consummed token    ____ first unconsummed token
--   /                            /
-- XXXXX  <spaces and comments> YYYYY
--      \____                    \____
--           :lineinfo_left()         :lineinfo_right()
----------------------------------------------------------------------
function lexer:lineinfo_right()
   return self:peek(1).lineinfo.first
end

function lexer:lineinfo_left()
   return self.lineinfo_last
end

)###"
R"###(

----------------------------------------------------------------------
-- Create a new lexstream.
----------------------------------------------------------------------
function lexer:newstream (src_or_stream, name)
   name = name or "?"
   if type(src_or_stream)=='table' then -- it's a stream
      return setmetatable ({ }, self) :takeover (src_or_stream)
   elseif type(src_or_stream)=='string' then -- it's a source string
      local src = src_or_stream
      local stream = { 
         src_name      = name;   -- Name of the file
         src           = src;    -- The source, as a single string
         peeked        = { };    -- Already peeked, but not discarded yet, tokens
         i             = 1;      -- Character offset in src
         line          = 1;      -- Current line number
         column_offset = 0;      -- distance from beginning of file to last '\n'
         attached_comments = { },-- comments accumulator
         lineinfo_last = { 1, 1, 1, name }
      }
      setmetatable (stream, self)

		      -- skip initial sharp-bang for unix scripts
      -- FIXME: redundant with mlp.chunk()
      if src and src :match "^#" then stream.i = src :find "\n" + 1 end
      return stream
   else
      assert(false, ":newstream() takes a source string or a stream, not a "..
                    type(src_or_stream))
   end
end

----------------------------------------------------------------------
-- if there's no ... args, return the token a (whose truth value is
-- true) if it's a `Keyword{ }, or nil.  If there are ... args, they
-- have to be strings. if the token a is a keyword, and it's content
-- is one of the ... args, then returns it (it's truth value is
-- true). If no a keyword or not in ..., return nil.
----------------------------------------------------------------------
function lexer:is_keyword (a, ...)
   if not a or a.tag ~= "Keyword" then return false end
   local words = {...}
   if #words == 0 then return a[1] end
   for _, w in ipairs (words) do
      if w == a[1] then return w end
   end
   return false
end

----------------------------------------------------------------------
-- Cause an error if the next token isn't a keyword whose content
-- is listed among ... args (which have to be strings).
----------------------------------------------------------------------
function lexer:check (...)
   local words = {...}
   local a = self:next()
   local function err ()
      error ("Got " .. tostring (a) .. 
             ", expected one of these keywords : '" ..
             _G.table.concat (words,"', '") .. "'") end
          
   if not a or a.tag ~= "Keyword" then err () end
   if #words == 0 then return a[1] end
   for _, w in ipairs (words) do
       if w == a[1] then return w end
   end
   err ()
end

----------------------------------------------------------------------
-- 
----------------------------------------------------------------------
function lexer:clone()
   local clone = {
      alpha = table.deep_copy(self.alpha),
      sym   = table.deep_copy(self.sym) }
   setmetatable(clone, self)
   clone.__index = clone
   return clone
end
)###";

std::string gg_lua = R"###(
----------------------------------------------------------------------
-- Metalua.
--
-- Summary: parser generator. Collection of higher order functors,
--   which allow to build and combine parsers. Relies on a lexer
--   that supports the same API as the one exposed in mll.lua.
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006-2008, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------

--------------------------------------------------------------------------------
--
-- Exported API:
--
-- Parser generators:
-- * [gg.sequence()]
-- * [gg.multisequence()]
-- * [gg.expr()]
-- * [gg.list()]
-- * [gg.onkeyword()]
-- * [gg.optkeyword()]
--
-- Other functions: 
-- * [gg.parse_error()]
-- * [gg.make_parser()]
-- * [gg.is_parser()]
--
--------------------------------------------------------------------------------

module("gg", package.seeall)

-------------------------------------------------------------------------------
-- parser metatable, which maps __call to method parse, and adds some
-- error tracing boilerplate.
-------------------------------------------------------------------------------
local parser_metatable = { }
function parser_metatable.__call (parser, lx, ...)
   --printf ("Call parser %q of type %q", parser.name or "?", parser.kind)
   if mlc.metabugs then 
      return parser:parse (lx, ...) 
      --local x = parser:parse (lx, ...) 
      --printf ("Result of parser %q: %s", 
      --        parser.name or "?",
      --        _G.table.tostring(x, "nohash", 80))
      --return x
   else
      local li = lx:lineinfo_right() or { "?", "?", "?", "?" }
      local status, ast = pcall (parser.parse, parser, lx, ...)      
      if status then return ast else
         error (string.format ("%s\n - (l.%s, c.%s, k.%s) in parser %s", 
                               ast:strmatch "gg.lua:%d+: (.*)" or ast,
                               li[1], li[2], li[3], parser.name or parser.kind))
      end
   end
end

-------------------------------------------------------------------------------
-- Turn a table into a parser, mainly by setting the metatable.
-------------------------------------------------------------------------------
function make_parser(kind, p)
   p.kind = kind
   if not p.transformers then p.transformers = { } end
   function p.transformers:add (x)
      table.insert (self, x)
   end
   setmetatable (p, parser_metatable)
   return p
end

-------------------------------------------------------------------------------
-- Return true iff [x] is a parser.
-- If it's a gg-generated parser, return the name of its kind.
-------------------------------------------------------------------------------
function is_parser (x)
   return type(x)=="function" or getmetatable(x)==parser_metatable and x.kind
end

-------------------------------------------------------------------------------
-- Parse a sequence, without applying builder nor transformers
-------------------------------------------------------------------------------
local function raw_parse_sequence (lx, p)
   local r = { }
   for i=1, #p do
      e=p[i]
      if type(e) == "string" then 
         if not lx:is_keyword (lx:next(), e) then
            parse_error (lx, "Keyword '%s' expected", e) end
      elseif is_parser (e) then
         table.insert (r, e (lx)) 
      else 
         gg.parse_error (lx,"Sequence `%s': element #%i is not a string "..
                         "nor a parser: %s", 
                         p.name, i, table.tostring(e))
      end
   end
   return r
end

-------------------------------------------------------------------------------
-- Parse a multisequence, without applying multisequence transformers.
-- The sequences are completely parsed.
-------------------------------------------------------------------------------
local function raw_parse_multisequence (lx, sequence_table, default)
   local seq_parser = sequence_table[lx:is_keyword(lx:peek())]
   if seq_parser  then return seq_parser (lx)
   elseif default then return default (lx)
   else return false end
end

-------------------------------------------------------------------------------
-- Applies all transformers listed in parser on ast.
-------------------------------------------------------------------------------
local function transform (ast, parser, fli, lli)
   if parser.transformers then
      for _, t in ipairs (parser.transformers) do ast = t(ast) or ast end
   end
   if type(ast) == 'table'then
      local ali = ast.lineinfo
      if not ali or ali.first~=fli or ali.last~=lli then
         ast.lineinfo = { first = fli, last = lli }
      end
   end
   return ast
end

-------------------------------------------------------------------------------
-- Generate a tracable parsing error (not implemented yet)
-------------------------------------------------------------------------------
function parse_error(lx, fmt, ...)
   local li = lx:lineinfo_left() or {-1,-1,-1, "<unknown file>"}
   local msg  = string.format("line %i, char %i: "..fmt, li[1], li[2], ...)   
   local src = lx.src
   if li[3]>0 and src then
      local i, j = li[3], li[3]
      while src:sub(i,i) ~= '\n' and i>=0    do i=i-1 end
      while src:sub(j,j) ~= '\n' and j<=#src do j=j+1 end      
      local srcline = src:sub (i+1, j-1)
      local idx  = string.rep (" ", li[2]).."^"
      msg = string.format("%s\n>>> %s\n>>> %s", msg, srcline, idx)
   end
   error(msg)
end
   
-------------------------------------------------------------------------------
--
-- Sequence parser generator
--
-------------------------------------------------------------------------------
-- Input fields:
--
-- * [builder]: how to build an AST out of sequence parts. let [x] be the list
--   of subparser results (keywords are simply omitted). [builder] can be:
--    - [nil], in which case the result of parsing is simply [x]
--    - a string, which is then put as a tag on [x]
--    - a function, which takes [x] as a parameter and returns an AST.
--
-- * [name]: the name of the parser. Used for debug messages
--
-- * [transformers]: a list of AST->AST functions, applied in order on ASTs
--   returned by the parser.
--
-- * Table-part entries corresponds to keywords (strings) and subparsers 
--   (function and callable objects).
--
-- After creation, the following fields are added:
-- * [parse] the parsing function lexer->AST
-- * [kind] == "sequence"
-- * [name] is set, if it wasn't in the input.
--
-------------------------------------------------------------------------------
function sequence (p)
   make_parser ("sequence", p)

		   -------------------------------------------------------------------
   -- Parsing method
   -------------------------------------------------------------------
   function p:parse (lx)
      -- Raw parsing:
      local fli = lx:lineinfo_right()
      local seq = raw_parse_sequence (lx, self)
      local lli = lx:lineinfo_left()

		      -- Builder application:
      local builder, tb = self.builder, type (self.builder)
      if tb == "string" then seq.tag = builder
      elseif tb == "function" or builder and builder.__call then seq = builder(seq)
      elseif builder == nil then -- nothing
      else error ("Invalid builder of type "..tb.." in sequence") end
      seq = transform (seq, self, fli, lli)
      assert (not seq or seq.lineinfo)
      return seq
   end

		   -------------------------------------------------------------------
   -- Construction
   -------------------------------------------------------------------
   -- Try to build a proper name
   if not p.name and type(p[1])=="string" then 
      p.name = p[1].." ..." 
      if type(p[#p])=="string" then p.name = p.name .. " " .. p[#p] end
   else
      p.name = "<anonymous>"
   end

		   return p
end --</sequence>


-------------------------------------------------------------------------------
--
-- Multiple, keyword-driven, sequence parser generator
--
-------------------------------------------------------------------------------
-- in [p], useful fields are:
--
-- * [transformers]: as usual
--
-- * [name]: as usual
--
-- * Table-part entries must be sequence parsers, or tables which can
--   be turned into a sequence parser by [gg.sequence]. These
--   sequences must start with a keyword, and this initial keyword
--   must be different for each sequence.  The table-part entries will
--   be removed after [gg.multisequence] returns.
--
-- * [default]: the parser to run if the next keyword in the lexer is
--   none of the registered initial keywords. If there's no default
--   parser and no suitable initial keyword, the multisequence parser
--   simply returns [false].
--
-- After creation, the following fields are added:
--
-- * [parse] the parsing function lexer->AST
--
-- * [sequences] the table of sequences, indexed by initial keywords.
--
-- * [add] method takes a sequence parser or a config table for
--   [gg.sequence], and adds/replaces the corresponding sequence
--   parser. If the keyword was already used, the former sequence is
--   removed and a warning is issued.
--
-- * [get] method returns a sequence by its initial keyword
--
-- * [kind] == "multisequence"
--
-------------------------------------------------------------------------------
function multisequence (p)   
   make_parser ("multisequence", p)

		   -------------------------------------------------------------------
   -- Add a sequence (might be just a config table for [gg.sequence])
   -------------------------------------------------------------------
   function p:add (s)
      -- compile if necessary:
      local keyword = s[1]
      if not is_parser(s) then sequence(s) end
      if is_parser(s) ~= 'sequence' or type(keyword) ~= "string" then 
         if self.default then -- two defaults
            error ("In a multisequence parser, all but one sequences "..
                   "must start with a keyword")
         else self.default = s end -- first default
      elseif self.sequences[keyword] then -- duplicate keyword
         eprintf (" *** Warning: keyword %q overloaded in multisequence ***", keyword)
         self.sequences[keyword] = s
      else -- newly caught keyword
         self.sequences[keyword] = s
      end
   end -- </multisequence.add>

		   -------------------------------------------------------------------
   -- Get the sequence starting with this keyword. [kw :: string]
   -------------------------------------------------------------------
   function p:get (kw) return self.sequences [kw] end

		   -------------------------------------------------------------------
   -- Remove the sequence starting with keyword [kw :: string]
   -------------------------------------------------------------------
   function p:del (kw) 
      if not self.sequences[kw] then 
         eprintf("*** Warning: trying to delete sequence starting "..
                 "with %q from a multisequence having no such "..
                 "entry ***", kw) end
      local removed = self.sequences[kw]
      self.sequences[kw] = nil 
      return removed
   end

		   -------------------------------------------------------------------
   -- Parsing method
   -------------------------------------------------------------------
   function p:parse (lx)
      local fli = lx:lineinfo_right()
      local x = raw_parse_multisequence (lx, self.sequences, self.default)
      local lli = lx:lineinfo_left()
      return transform (x, self, fli, lli)
   end

		   -------------------------------------------------------------------
   -- Construction
   -------------------------------------------------------------------
   -- Register the sequences passed to the constructor. They're going
   -- from the array part of the parser to the hash part of field
   -- [sequences]
   p.sequences = { }
   for i=1, #p do p:add (p[i]); p[i] = nil end

		   -- FIXME: why is this commented out?
   --if p.default and not is_parser(p.default) then sequence(p.default) end
   return p
end --</multisequence>


-------------------------------------------------------------------------------
--
-- Expression parser generator
--
-------------------------------------------------------------------------------
--
-- Expression configuration relies on three tables: [prefix], [infix]
-- and [suffix]. Moreover, the primary parser can be replaced by a
-- table: in this case the [primary] table will be passed to
-- [gg.multisequence] to create a parser.
--
-- Each of these tables is a modified multisequence parser: the
-- differences with respect to regular multisequence config tables are:
--
-- * the builder takes specific parameters:
--   - for [prefix], it takes the result of the prefix sequence parser,
--     and the prefixed expression
--   - for [infix], it takes the left-hand-side expression, the results 
--     of the infix sequence parser, and the right-hand-side expression.
--   - for [suffix], it takes the suffixed expression, and theresult 
--     of the suffix sequence parser.
--
-- * the default field is a list, with parameters:
--   - [parser] the raw parsing function
--   - [transformers], as usual
--   - [prec], the operator's precedence
--   - [assoc] for [infix] table, the operator's associativity, which
--     can be "left", "right" or "flat" (default to left)
--
-- In [p], useful fields are:
-- * [transformers]: as usual
-- * [name]: as usual
-- * [primary]: the atomic expression parser, or a multisequence config 
--   table (mandatory)
-- * [prefix]:  prefix  operators config table, see above.
-- * [infix]:   infix   operators config table, see above.
-- * [suffix]: suffix operators config table, see above.
--
-- After creation, these fields are added:
-- * [kind] == "expr"
-- * [parse] as usual
-- * each table is turned into a multisequence, and therefore has an 
--   [add] method
--
-------------------------------------------------------------------------------
function expr (p)
   make_parser ("expr", p)

		   -------------------------------------------------------------------
   -- parser method.
   -- In addition to the lexer, it takes an optional precedence:
   -- it won't read expressions whose precedence is lower or equal
   -- to [prec].
   -------------------------------------------------------------------
   function p:parse (lx, prec)
      prec = prec or 0

		      ------------------------------------------------------
      -- Extract the right parser and the corresponding
      -- options table, for (pre|in|suff)fix operators.
      -- Options include prec, assoc, transformers.
      ------------------------------------------------------
      local function get_parser_info (tab)
         local p2 = tab:get (lx:is_keyword (lx:peek()))
         if p2 then -- keyword-based sequence found
            local function parser(lx) return raw_parse_sequence(lx, p2) end
            return parser, p2
         else -- Got to use the default parser
            local d = tab.default
            if d then return d.parse or d.parser, d
            else return false, false end
         end
      end

	)###"
	R"###(

		      ------------------------------------------------------
      -- Look for a prefix sequence. Multiple prefixes are
      -- handled through the recursive [p.parse] call.
      -- Notice the double-transform: one for the primary
      -- expr, and one for the one with the prefix op.
      ------------------------------------------------------
      local function handle_prefix ()
         local fli = lx:lineinfo_right()
         local p2_func, p2 = get_parser_info (self.prefix)
         local op = p2_func and p2_func (lx)
         if op then -- Keyword-based sequence found
            local ili = lx:lineinfo_right() -- Intermediate LineInfo
            local e = p2.builder (op, self:parse (lx, p2.prec))
            local lli = lx:lineinfo_left()
            return transform (transform (e, p2, ili, lli), self, fli, lli)
         else -- No prefix found, get a primary expression         
            local e = self.primary(lx)
            local lli = lx:lineinfo_left()
            return transform (e, self, fli, lli)
         end
      end --</expr.parse.handle_prefix>

		      ------------------------------------------------------
      -- Look for an infix sequence+right-hand-side operand.
      -- Return the whole binary expression result,
      -- or false if no operator was found.
      ------------------------------------------------------
      local function handle_infix (e)
         local p2_func, p2 = get_parser_info (self.infix)
         if not p2 then return false end

		         -----------------------------------------
         -- Handle flattening operators: gather all operands
         -- of the series in [list]; when a different operator 
         -- is found, stop, build from [list], [transform] and
         -- return.
         -----------------------------------------
         if (not p2.prec or p2.prec>prec) and p2.assoc=="flat" then
            local fli = lx:lineinfo_right()
            local pflat, list = p2, { e }
            repeat
               local op = p2_func(lx)
               if not op then break end
               table.insert (list, self:parse (lx, p2.prec))
               local _ -- We only care about checking that p2==pflat
               _, p2 = get_parser_info (self.infix)
            until p2 ~= pflat
            local e2 = pflat.builder (list)
            local lli = lx:lineinfo_left()
            return transform (transform (e2, pflat, fli, lli), self, fli, lli)
 
         -----------------------------------------
         -- Handle regular infix operators: [e] the LHS is known,
         -- just gather the operator and [e2] the RHS.
         -- Result goes in [e3].
         -----------------------------------------
         elseif p2.prec and p2.prec>prec or 
                p2.prec==prec and p2.assoc=="right" then
            local fli = e.lineinfo.first -- lx:lineinfo_right()
            local op = p2_func(lx)
            if not op then return false end
            local e2 = self:parse (lx, p2.prec)
            local e3 = p2.builder (e, op, e2)
            local lli = lx:lineinfo_left()
            return transform (transform (e3, p2, fli, lli), self, fli, lli)

		         -----------------------------------------
         -- Check for non-associative operators, and complain if applicable. 
         -----------------------------------------
         elseif p2.assoc=="none" and p2.prec==prec then
            parser_error (lx, "non-associative operator!")

		         -----------------------------------------
         -- No infix operator suitable at that precedence
         -----------------------------------------
         else return false end

		      end --</expr.parse.handle_infix>

		      ------------------------------------------------------
      -- Look for a suffix sequence.
      -- Return the result of suffix operator on [e],
      -- or false if no operator was found.
      ------------------------------------------------------
      local function handle_suffix (e)
         -- FIXME bad fli, must take e.lineinfo.first
         local p2_func, p2 = get_parser_info (self.suffix)
         if not p2 then return false end
         if not p2.prec or p2.prec>=prec then
            --local fli = lx:lineinfo_right()
            local fli = e.lineinfo.first
            local op = p2_func(lx)
            if not op then return false end
            local lli = lx:lineinfo_left()
            e = p2.builder (e, op)
            e = transform (transform (e, p2, fli, lli), self, fli, lli)
            return e
         end
         return false
      end --</expr.parse.handle_suffix>

		      ------------------------------------------------------
      -- Parser body: read suffix and (infix+operand) 
      -- extensions as long as we're able to fetch more at
      -- this precedence level.
      ------------------------------------------------------
      local e = handle_prefix()
      repeat
         local x = handle_suffix (e); e = x or e
         local y = handle_infix   (e); e = y or e
      until not (x or y)

		      -- No transform: it already happened in operators handling
      return e
   end --</expr.parse>

		   -------------------------------------------------------------------
   -- Construction
   -------------------------------------------------------------------
   if not p.primary then p.primary=p[1]; p[1]=nil end
   for _, t in ipairs{ "primary", "prefix", "infix", "suffix" } do
      if not p[t] then p[t] = { } end
      if not is_parser(p[t]) then multisequence(p[t]) end
   end
   function p:add(...) return self.primary:add(...) end
   return p
end --</expr>


-------------------------------------------------------------------------------
--
-- List parser generator
--
-------------------------------------------------------------------------------
-- In [p], the following fields can be provided in input:
--
-- * [builder]: takes list of subparser results, returns AST
-- * [transformers]: as usual
-- * [name]: as usual
--
-- * [terminators]: list of strings representing the keywords which
--   might mark the end of the list. When non-empty, the list is
--   allowed to be empty. A string is treated as a single-element
--   table, whose element is that string, e.g. ["do"] is the same as
--   [{"do"}].
--
-- * [separators]: list of strings representing the keywords which can
--   separate elements of the list. When non-empty, one of these
--   keyword has to be found between each element. Lack of a separator
--   indicates the end of the list. A string is treated as a
--   single-element table, whose element is that string, e.g. ["do"]
--   is the same as [{"do"}]. If [terminators] is empty/nil, then
--   [separators] has to be non-empty.
--
-- After creation, the following fields are added:
-- * [parse] the parsing function lexer->AST
-- * [kind] == "list"
--
-------------------------------------------------------------------------------
function list (p)
   make_parser ("list", p)

		   -------------------------------------------------------------------
   -- Parsing method
   -------------------------------------------------------------------
   function p:parse (lx)

		      ------------------------------------------------------
      -- Used to quickly check whether there's a terminator 
      -- or a separator immediately ahead
      ------------------------------------------------------
      local function peek_is_in (keywords) 
         return keywords and lx:is_keyword(lx:peek(), unpack(keywords)) end

		      local x = { }
      local fli = lx:lineinfo_right()

		      -- if there's a terminator to start with, don't bother trying
      if not peek_is_in (self.terminators) then 
         repeat table.insert (x, self.primary (lx)) -- read one element
         until
            -- First reason to stop: There's a separator list specified,
            -- and next token isn't one. Otherwise, consume it with [lx:next()]
            self.separators and not(peek_is_in (self.separators) and lx:next()) or
            -- Other reason to stop: terminator token ahead
            peek_is_in (self.terminators) or
            -- Last reason: end of file reached
            lx:peek().tag=="Eof"
      end

		      local lli = lx:lineinfo_left()
      
      -- Apply the builder. It can be a string, or a callable value, 
      -- or simply nothing.
      local b = self.builder
      if b then
         if type(b)=="string" then x.tag = b -- b is a string, use it as a tag
         elseif type(b)=="function" then x=b(x)
         else
            local bmt = getmetatable(b)
            if bmt and bmt.__call then x=b(x) end
         end
      end
      return transform (x, self, fli, lli)
   end --</list.parse>

		   -------------------------------------------------------------------
   -- Construction
   -------------------------------------------------------------------
   if not p.primary then p.primary = p[1]; p[1] = nil end
   if type(p.terminators) == "string" then p.terminators = { p.terminators }
   elseif p.terminators and #p.terminators == 0 then p.terminators = nil end
   if type(p.separators) == "string" then p.separators = { p.separators }
   elseif p.separators and #p.separators == 0 then p.separators = nil end

		   return p
end --</list>


-------------------------------------------------------------------------------
--
-- Keyword-conditionned parser generator
--
-------------------------------------------------------------------------------
-- 
-- Only apply a parser if a given keyword is found. The result of
-- [gg.onkeyword] parser is the result of the subparser (modulo
-- [transformers] applications).
--
-- lineinfo: the keyword is *not* included in the boundaries of the
-- resulting lineinfo. A review of all usages of gg.onkeyword() in the
-- implementation of metalua has shown that it was the appropriate choice
-- in every case.
--
-- Input fields:
--
-- * [name]: as usual
--
-- * [transformers]: as usual
--
-- * [peek]: if non-nil, the conditionning keyword is left in the lexeme
--   stream instead of being consumed.
--
-- * [primary]: the subparser. 
--
-- * [keywords]: list of strings representing triggering keywords.
--
-- * Table-part entries can contain strings, and/or exactly one parser.
--   Strings are put in [keywords], and the parser is put in [primary].
--
-- After the call, the following fields will be set:
--   
-- * [parse] the parsing method
-- * [kind] == "onkeyword"
-- * [primary]
-- * [keywords]
--
-------------------------------------------------------------------------------
function onkeyword (p)
   make_parser ("onkeyword", p)

		   -------------------------------------------------------------------
   -- Parsing method
   -------------------------------------------------------------------
   function p:parse(lx)
      if lx:is_keyword (lx:peek(), unpack(self.keywords)) then
         --local fli = lx:lineinfo_right()
         if not self.peek then lx:next() end
         local content = self.primary (lx)
         --local lli = lx:lineinfo_left()
         local fli, lli = content.lineinfo.first, content.lineinfo.last
         return transform (content, p, fli, lli)
      else return false end
   end

		   -------------------------------------------------------------------
   -- Construction
   -------------------------------------------------------------------
   if not p.keywords then p.keywords = { } end
   for _, x in ipairs(p) do
      if type(x)=="string" then table.insert (p.keywords, x)
      else assert (not p.primary and is_parser (x)); p.primary = x end
   end
   if not next (p.keywords) then 
      eprintf("Warning, no keyword to trigger gg.onkeyword") end
   assert (p.primary, 'no primary parser in gg.onkeyword')
   return p
end --</onkeyword>


-------------------------------------------------------------------------------
--
-- Optional keyword consummer pseudo-parser generator
--
-------------------------------------------------------------------------------
--
-- This doesn't return a real parser, just a function. That function parses
-- one of the keywords passed as parameters, and returns it. It returns 
-- [false] if no matching keyword is found.
--
-- Notice that tokens returned by lexer already carry lineinfo, therefore
-- there's no need to add them, as done usually through transform() calls.
-------------------------------------------------------------------------------
function optkeyword (...)
   local args = {...}
   if type (args[1]) == "table" then 
      assert (#args == 1)
      args = args[1]
   end
   for _, v in ipairs(args) do assert (type(v)=="string") end
   return function (lx)
      local x = lx:is_keyword (lx:peek(), unpack (args))
      if x then lx:next(); return x
      else return false end
   end
end


-------------------------------------------------------------------------------
--
-- Run a parser with a special lexer
--
-------------------------------------------------------------------------------
--
-- This doesn't return a real parser, just a function.
-- First argument is the lexer class to be used with the parser,
-- 2nd is the parser itself.
-- The resulting parser returns whatever the argument parser does.
--
-------------------------------------------------------------------------------
function with_lexer(new_lexer, parser)

   -------------------------------------------------------------------
   -- Most gg functions take their parameters in a table, so it's 
   -- better to silently accept when with_lexer{ } is called with
   -- its arguments in a list:
   -------------------------------------------------------------------
   if not parser and #new_lexer==2 and type(new_lexer[1])=='table' then
      return with_lexer(unpack(new_lexer))
   end

		   -------------------------------------------------------------------
   -- Save the current lexer, switch it for the new one, run the parser,
   -- restore the previous lexer, even if the parser caused an error.
   -------------------------------------------------------------------
   return function (lx)
      local old_lexer = getmetatable(lx)
      lx:sync()
      setmetatable(lx, new_lexer)
      local status, result = pcall(parser, lx)
      lx:sync()
      setmetatable(lx, old_lexer)
      if status then return result else error(result) end
   end
end
)###";

std::string mlplexer_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mll.lua,v 1.3 2006/11/15 09:07:50 fab13n Exp $
--
-- Summary: Source file lexer. ~~Currently only works on strings.
-- Some API refactoring is needed.
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006-2007, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------

module ("mlp", package.seeall)

local mlp_lexer = lexer.lexer:clone()

local keywords = {
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while",
    "...", "..", "==", ">=", "<=", "~=", 
    "+{", "-{" }
 
for w in values(keywords) do mlp_lexer:add(w) end

_M.lexer = mlp_lexer

)###";

std::string mlpmisc_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mlp_misc.lua,v 1.6 2006/11/15 09:07:50 fab13n Exp $
--
-- Summary: metalua parser, miscellaneous utility functions.
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------
-- History:
-- $Log: mlp_misc.lua,v $
-- Revision 1.6  2006/11/15 09:07:50  fab13n
-- debugged meta operators.
-- Added command line options handling.
--
-- Revision 1.5  2006/11/10 02:11:17  fab13n
-- compiler faithfulness to 5.1 improved
-- gg.expr extended
-- mlp.expr refactored
--
-- Revision 1.4  2006/11/09 09:39:57  fab13n
-- some cleanup
--
-- Revision 1.3  2006/11/07 04:38:00  fab13n
-- first bootstrapping version.
--
-- Revision 1.2  2006/11/05 15:08:34  fab13n
-- updated code generation, to be compliant with 5.1
--
----------------------------------------------------------------------

--------------------------------------------------------------------------------
--
-- Exported API:
-- * [mlp.fget()]
-- * [mlp.id()]
-- * [mlp.opt_id()]
-- * [mlp.id_list()]
-- * [mlp.gensym()]
-- * [mlp.string()]
-- * [mlp.opt_string()]
-- * [mlp.id2string()]
--
--------------------------------------------------------------------------------

--require "gg"
--require "mll"

module ("mlp", package.seeall)

--------------------------------------------------------------------------------
-- returns a function that takes the [n]th element of a table.
-- if [tag] is provided, then this element is expected to be a
-- table, and this table receives a "tag" field whose value is
-- set to [tag].
--
-- The primary purpose of this is to generate builders for
-- grammar generators. It has little purpose in metalua, as lambda has
-- a lightweight syntax.
--------------------------------------------------------------------------------

function fget (n, tag) 
   assert (type (n) == "number")
   if tag then
      assert (type (tag) == "string")
      return function (x) 
         assert (type (x[n]) == "table")       
         return {tag=tag, unpack(x[n])} end 
   else
      return function (x) return x[n] end 
   end
end


--------------------------------------------------------------------------------
-- Try to read an identifier (possibly as a splice), or return [false] if no
-- id is found.
--------------------------------------------------------------------------------
function opt_id (lx)
   local a = lx:peek();
   if lx:is_keyword (a, "-{") then
      local v = gg.sequence{ "-{", splice_content, "}" } (lx) [1]
      if v.tag ~= "Id" and v.tag ~= "Splice" then
         gg.parse_error(lx,"Bad id splice")
      end
      return v
   elseif a.tag == "Id" then return lx:next()
   else return false end
end

--------------------------------------------------------------------------------
-- Mandatory reading of an id: causes an error if it can't read one.
--------------------------------------------------------------------------------
function id (lx)
   return opt_id (lx) or gg.parse_error(lx,"Identifier expected")
end

--------------------------------------------------------------------------------
-- Common helper function
--------------------------------------------------------------------------------
id_list = gg.list { primary = mlp.id, separators = "," }

--------------------------------------------------------------------------------
-- Symbol generator: [gensym()] returns a guaranteed-to-be-unique identifier.
-- The main purpose is to avoid variable capture in macros.
--
-- If a string is passed as an argument, theis string will be part of the
-- id name (helpful for macro debugging)
--------------------------------------------------------------------------------
local gensymidx = 0

function gensym (arg)
   gensymidx = gensymidx + 1
   return { tag="Id", _G.string.format(".%i.%s", gensymidx, arg or "")}
end

--------------------------------------------------------------------------------
-- Converts an identifier into a string. Hopefully one day it'll handle
-- splices gracefully, but that proves quite tricky.
--------------------------------------------------------------------------------
function id2string (id)
   --print("id2string:", disp.ast(id))
   if id.tag == "Id" then id.tag = "String"; return id
   elseif id.tag == "Splice" then
      assert (in_a_quote, "can't do id2string on an outermost splice")
      error ("id2string on splice not implemented")
      -- Evaluating id[1] will produce `Id{ xxx },
      -- and we want it to produce `String{ xxx }
      -- Morally, this is what I want:
      -- return `String{ `Index{ `Splice{ id[1] }, `Number 1 } }
      -- That is, without sugar:
      return {tag="String",  {tag="Index", {tag="Splice", id[1] }, 
                                           {tag="Number", 1 } } }
   else error ("Identifier expected: "..table.tostring(id)) end
end

--------------------------------------------------------------------------------
-- Read a string, possibly spliced, or return an error if it can't
--------------------------------------------------------------------------------
function string (lx)
   local a = lx:peek()
   if lx:is_keyword (a, "-{") then
      local v = gg.sequence{ "-{", splice_content, "}" } (lx) [1]
      if v.tag ~= "" and v.tag ~= "Splice" then
         gg.parse_error(lx,"Bad string splice")
      end
      return v
   elseif a.tag == "String" then return lx:next()
   else error "String expected" end
end

--------------------------------------------------------------------------------
-- Try to read a string, or return false if it can't. No splice allowed.
--------------------------------------------------------------------------------
function opt_string (lx)
   return lx:peek().tag == "String" and lx:next()
end
   
--------------------------------------------------------------------------------
-- Chunk reader: block + Eof
--------------------------------------------------------------------------------
function skip_initial_sharp_comment (lx)
   -- Dirty hack: I'm happily fondling lexer's private parts
   -- FIXME: redundant with lexer:newstream()
   lx :sync()
   local i = lx.src:match ("^#.-\n()", lx.i)
   if i then lx.i, lx.column_offset, lx.line = i, i, lx.line+1 end
end

local function _chunk (lx)
   if lx:peek().tag == 'Eof' then return { } -- handle empty files
   else 
      skip_initial_sharp_comment (lx)
      local chunk = block (lx)
      if lx:peek().tag ~= "Eof" then error "End-of-file expected" end
      return chunk
   end
end

-- chunk is wrapped in a sequence so that it has a "transformer" field.
chunk = gg.sequence { _chunk, builder = unpack }
)###";

std::string mlptable_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mlp_table.lua,v 1.5 2006/11/10 02:11:17 fab13n Exp $
--
-- Summary: metalua parser, table constructor parser. This is part 
--   of thedefinition of module [mlp].
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------
-- History:
-- $Log: mlp_table.lua,v $
-- Revision 1.5  2006/11/10 02:11:17  fab13n
-- compiler faithfulness to 5.1 improved
-- gg.expr extended
-- mlp.expr refactored
--
-- Revision 1.4  2006/11/09 09:39:57  fab13n
-- some cleanup
--
-- Revision 1.3  2006/11/07 04:38:00  fab13n
-- first bootstrapping version.
--
-- Revision 1.2  2006/11/05 15:08:34  fab13n
-- updated code generation, to be compliant with 5.1
--
----------------------------------------------------------------------

--------------------------------------------------------------------------------
--
-- Exported API:
-- * [mlp.table_field()]
-- * [mlp.table_content()]
-- * [mlp.table()]
--
-- KNOWN BUG: doesn't handle final ";" or "," before final "}"
--
--------------------------------------------------------------------------------

--require "gg"
--require "mll"
--require "mlp_misc"

module ("mlp", package.seeall)

--------------------------------------------------------------------------------
-- eta expansion to break circular dependencies:
--------------------------------------------------------------------------------
local function _expr (lx) return expr(lx) end

--------------------------------------------------------------------------------
-- [[key] = value] table field definition
--------------------------------------------------------------------------------
local bracket_field = gg.sequence{ "[", _expr, "]", "=", _expr, builder = "Pair" }

--------------------------------------------------------------------------------
-- [id = value] or [value] table field definition;
-- [[key]=val] are delegated to [bracket_field()]
--------------------------------------------------------------------------------
function table_field (lx)
   if lx:is_keyword (lx:peek(), "[") then return bracket_field (lx) end
   local e = _expr (lx)
   if lx:is_keyword (lx:peek(), "=") then 
      lx:next(); -- skip the "="
      local key = id2string(e)
      local val = _expr(lx)
      local r = { tag="Pair", key, val } 
      r.lineinfo = { first = key.lineinfo.first, last = val.lineinfo.last }
      return r
   else return e end
end

local function _table_field(lx) return table_field(lx) end

--------------------------------------------------------------------------------
-- table constructor, without enclosing braces; returns a full table object
--------------------------------------------------------------------------------
table_content = gg.list { _table_field, 
   separators = { ",", ";" }, terminators = "}", builder = "Table" }

		local function _table_content(lx) return table_content(lx) end

		--------------------------------------------------------------------------------
-- complete table constructor including [{...}]
--------------------------------------------------------------------------------
table = gg.sequence{ "{", _table_content, "}", builder = fget(1) }
)###";

std::string mlpmeta_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mlp_meta.lua,v 1.4 2006/11/15 09:07:50 fab13n Exp $
--
-- Summary: Meta-operations: AST quasi-quoting and splicing
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------


--------------------------------------------------------------------------------
--
-- Exported API:
-- * [mlp.splice_content()]
-- * [mlp.quote_content()]
--
--------------------------------------------------------------------------------

module ("mlp", package.seeall)

--------------------------------------------------------------------------------
-- External splicing: compile an AST into a chunk, load and evaluate
-- that chunk, and replace the chunk by its result (which must also be
-- an AST).
--------------------------------------------------------------------------------

function splice (ast)
   local f = mlc.function_of_ast(ast, '=splice')
   local result=f()
   return result
end

--------------------------------------------------------------------------------
-- Going from an AST to an AST representing that AST
-- the only key being lifted in this version is ["tag"]
--------------------------------------------------------------------------------
function quote (t)
   --print("QUOTING:", _G.table.tostring(t, 60))
   local cases = { }
   function cases.table (t)
      local mt = { tag = "Table" }
      --_G.table.insert (mt, { tag = "Pair", quote "quote", { tag = "True" } })
      if t.tag == "Splice" then
         assert (#t==1, "Invalid splice")
         local sp = t[1]
         return sp
      elseif t.tag then
         _G.table.insert (mt, { tag = "Pair", quote "tag", quote (t.tag) })
      end
      for _, v in ipairs (t) do
         _G.table.insert (mt, quote(v))
      end
      return mt
   end
   function cases.number (t) return { tag = "Number", t, quote = true } end
   function cases.string (t) return { tag = "String", t, quote = true } end
   return cases [ type (t) ] (t)
end

--------------------------------------------------------------------------------
-- when this variable is false, code inside [-{...}] is compiled and
-- avaluated immediately. When it's true (supposedly when we're
-- parsing data inside a quasiquote), [-{foo}] is replaced by
-- [`Splice{foo}], which will be unpacked by [quote()].
--------------------------------------------------------------------------------
in_a_quote = false

--------------------------------------------------------------------------------
-- Parse the inside of a "-{ ... }"
--------------------------------------------------------------------------------
function splice_content (lx)
   local parser_name = "expr"
   if lx:is_keyword (lx:peek(2), ":") then
      local a = lx:next()
      lx:next() -- skip ":"
      assert (a.tag=="Id", "Invalid splice parser name")
      parser_name = a[1]
   end
   local ast = mlp[parser_name](lx)
   if in_a_quote then
      --printf("SPLICE_IN_QUOTE:\n%s", _G.table.tostring(ast, "nohash", 60))
      return { tag="Splice", ast }
   else
      if parser_name == "expr" then ast = { { tag="Return", ast } }
      elseif parser_name == "stat"  then ast = { ast }
      elseif parser_name ~= "block" then
         error ("splice content must be an expr, stat or block") end
      --printf("EXEC THIS SPLICE:\n%s", _G.table.tostring(ast, "nohash", 60))
      return splice (ast)
   end
end

--------------------------------------------------------------------------------
-- Parse the inside of a "+{ ... }"
--------------------------------------------------------------------------------
function quote_content (lx)
   local parser 
   if lx:is_keyword (lx:peek(2), ":") then -- +{parser: content }
      parser = mlp[id(lx)[1]]
      lx:next()
   else -- +{ content }
      parser = mlp.expr
   end

		   local prev_iq = in_a_quote
   in_a_quote = true
   --print("IN_A_QUOTE")
   local content = parser (lx)
   local q_content = quote (content)
   in_a_quote = prev_iq
   return q_content
end
)###";

std::string mlpexpr_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mlp_expr.lua,v 1.7 2006/11/15 09:07:50 fab13n Exp $
--
-- Summary: metalua parser, expression parser. This is part of the
--   definition of module [mlp].
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------
-- History:
-- $Log: mlp_expr.lua,v $
-- Revision 1.7  2006/11/15 09:07:50  fab13n
-- debugged meta operators.
-- Added command line options handling.
--
-- Revision 1.6  2006/11/10 02:11:17  fab13n
-- compiler faithfulness to 5.1 improved
-- gg.expr extended
-- mlp.expr refactored
--
-- Revision 1.5  2006/11/09 09:39:57  fab13n
-- some cleanup
--
-- Revision 1.4  2006/11/07 21:29:02  fab13n
-- improved quasi-quoting
--
-- Revision 1.3  2006/11/07 04:38:00  fab13n
-- first bootstrapping version.
--
-- Revision 1.2  2006/11/05 15:08:34  fab13n
-- updated code generation, to be compliant with 5.1
--
----------------------------------------------------------------------

--------------------------------------------------------------------------------
--
-- Exported API:
-- * [mlp.expr()]
-- * [mlp.expr_list()]
-- * [mlp.func_val()]
--
--------------------------------------------------------------------------------

--require "gg"
--require "mlp_misc"
--require "mlp_table"
--require "mlp_meta"

--------------------------------------------------------------------------------
-- These function wrappers (eta-expansions ctually) are just here to break
-- some circular dependencies between mlp_xxx.lua files.
--------------------------------------------------------------------------------
local function _expr (lx) return mlp.expr (lx)  end
local function _table_content (lx) return mlp.table_content (lx) end
local function block (lx) return mlp.block (lx) end
local function stat  (lx) return mlp.stat (lx)  end

module ("mlp", package.seeall)

--------------------------------------------------------------------------------
-- Non-empty expression list. Actually, this isn't used here, but that's
-- handy to give to users.
--------------------------------------------------------------------------------
expr_list = gg.list{ _expr, separators = "," }

--------------------------------------------------------------------------------
-- Helpers for function applications / method applications
--------------------------------------------------------------------------------
func_args_content = gg.list { 
   name = "function arguments",
   _expr, separators = ",", terminators = ")" } 

		-- Used to parse methods
method_args = gg.multisequence{
   name = "function argument(s)",
   { "{", table_content, "}" },
   { "(", func_args_content, ")", builder = fget(1) },
   default = function(lx) local r = opt_string(lx); return r and {r} or { } end }

		--------------------------------------------------------------------------------
-- [func_val] parses a function, from opening parameters parenthese to
-- "end" keyword included. Used for anonymous functions as well as
-- function declaration statements (both local and global).
--
-- It's wrapped in a [_func_val] eta expansion, so that when expr
-- parser uses the latter, they will notice updates of [func_val]
-- definitions.
--------------------------------------------------------------------------------
func_params_content = gg.list{ name="function parameters",
   gg.multisequence{ { "...", builder = "Dots" }, default = id },
   separators  = ",", terminators = {")", "|"} } 

		local _func_params_content = function (lx) return func_params_content(lx) end

		func_val = gg.sequence { name="function body",
   "(", func_params_content, ")", block, "end", builder = "Function" }

		local _func_val = function (lx) return func_val(lx) end

		--------------------------------------------------------------------------------
-- Default parser for primary expressions
--------------------------------------------------------------------------------
function id_or_literal (lx)
   local a = lx:next()
   if a.tag~="Id" and a.tag~="String" and a.tag~="Number" then
      gg.parse_error (lx, "Unexpected expr token %s",
                      _G.table.tostring (a, 'nohash'))
   end
   return a
end


--------------------------------------------------------------------------------
-- Builder generator for operators. Wouldn't be worth it if "|x|" notation
-- were allowed, but then lua 5.1 wouldn't compile it 
--------------------------------------------------------------------------------

-- opf1 = |op| |_,a| `Op{ op, a }
local function opf1 (op) return 
   function (_,a) return { tag="Op", op, a } end end

		-- opf2 = |op| |a,_,b| `Op{ op, a, b }
local function opf2 (op) return 
   function (a,_,b) return { tag="Op", op, a, b } end end

		-- opf2r = |op| |a,_,b| `Op{ op, b, a } -- (args reversed)
local function opf2r (op) return 
   function (a,_,b) return { tag="Op", op, b, a } end end

		local function op_ne(a, _, b) 
   -- The first version guarantees to return the same code as Lua,
   -- but it relies on the non-standard 'ne' operator, which has been
   -- suppressed from the official AST grammar (although still supported
   -- in practice by the compiler).
   -- return { tag="Op", "ne", a, b }
   return { tag="Op", "not", { tag="Op", "eq", a, b, lineinfo= {
            first = a.lineinfo.first, last = b.lineinfo.last } } }
end
   

		--------------------------------------------------------------------------------
--
-- complete expression
--
--------------------------------------------------------------------------------

-- FIXME: set line number. In [expr] transformers probably

expr = gg.expr { name = "expression",

   primary = gg.multisequence{ name="expr primary",
      { "(", _expr, ")",           builder = "Paren" },
      { "function", _func_val,     builder = fget(1) },
      { "-{", splice_content, "}", builder = fget(1) },
      { "+{", quote_content, "}",  builder = fget(1) }, 
      { "nil",                     builder = "Nil" },
      { "true",                    builder = "True" },
      { "false",                   builder = "False" },
      { "...",                     builder = "Dots" },
      table,
      default = id_or_literal },

		   infix = { name="expr infix op",
      { "+",  prec = 60, builder = opf2 "add"  },
      { "-",  prec = 60, builder = opf2 "sub"  },
      { "*",  prec = 70, builder = opf2 "mul"  },
      { "/",  prec = 70, builder = opf2 "div"  },
      { "%",  prec = 70, builder = opf2 "mod"  },
      { "^",  prec = 90, builder = opf2 "pow",    assoc = "right" },
      { "..", prec = 40, builder = opf2 "concat", assoc = "right" },
      { "==", prec = 30, builder = opf2 "eq"  },
      { "~=", prec = 30, builder = op_ne  },
      { "<",  prec = 30, builder = opf2 "lt"  },
      { "<=", prec = 30, builder = opf2 "le"  },
      { ">",  prec = 30, builder = opf2r "lt"  },
      { ">=", prec = 30, builder = opf2r "le"  },
      { "and",prec = 20, builder = opf2 "and" },
      { "or", prec = 10, builder = opf2 "or"  } },

		   prefix = { name="expr prefix op",
      { "not", prec = 80, builder = opf1 "not" },
      { "#",   prec = 80, builder = opf1 "len" },
      { "-",   prec = 80, builder = opf1 "unm" } },

		   suffix = { name="expr suffix op",
      { "[", _expr, "]", builder = function (tab, idx) 
         return {tag="Index", tab, idx[1]} end},
      { ".", id, builder = function (tab, field) 
         return {tag="Index", tab, id2string(field[1])} end },
      { "(", func_args_content, ")", builder = function(f, args) 
         return {tag="Call", f, unpack(args[1])} end },
      { "{", _table_content, "}", builder = function (f, arg)
         return {tag="Call", f, arg[1]} end},
      { ":", id, method_args, builder = function (obj, post)
         return {tag="Invoke", obj, id2string(post[1]), unpack(post[2])} end},
      { "+{", quote_content, "}", builder = function (f, arg) 
         return {tag="Call", f,  arg[1] } end },
      default = { name="opt_string_arg", parse = mlp.opt_string, builder = function(f, arg) 
         return {tag="Call", f, arg } end } } }
)###";


std::string mlpstat_lua = R"###(
----------------------------------------------------------------------
-- Metalua:  $Id: mlp_stat.lua,v 1.7 2006/11/15 09:07:50 fab13n Exp $
--
-- Summary: metalua parser, statement/block parser. This is part of
--   the definition of module [mlp].
--
----------------------------------------------------------------------
--
-- Copyright (c) 2006, Fabien Fleutot <metalua@gmail.com>.
--
-- This software is released under the MIT Licence, see licence.txt
-- for details.
--
----------------------------------------------------------------------
--
----------------------------------------------------------------------

--------------------------------------------------------------------------------
--
-- Exports API:
-- * [mlp.stat()]
-- * [mlp.block()]
-- * [mlp.for_header()]
--
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- eta-expansions to break circular dependency
--------------------------------------------------------------------------------
local expr      = function (lx) return mlp.expr     (lx) end
local func_val  = function (lx) return mlp.func_val (lx) end
local expr_list = function (lx) return mlp.expr_list(lx) end

module ("mlp", package.seeall)

--------------------------------------------------------------------------------
-- List of all keywords that indicate the end of a statement block. Users are
-- likely to extend this list when designing extensions.
--------------------------------------------------------------------------------


local block_terminators = { "else", "elseif", "end", "until", ")", "}", "]" }

-- FIXME: this must be handled from within GG!!!
function block_terminators:add(x) 
   if type (x) == "table" then for _, y in ipairs(x) do self:add (y) end
   else _G.table.insert (self, x) end
end

--------------------------------------------------------------------------------
-- list of statements, possibly followed by semicolons
--------------------------------------------------------------------------------
block = gg.list {
   name        = "statements block",
   terminators = block_terminators,
   primary     = function (lx)
      -- FIXME use gg.optkeyword()
      local x = stat (lx)
      if lx:is_keyword (lx:peek(), ";") then lx:next() end
      return x
   end }

		--------------------------------------------------------------------------------
-- Helper function for "return <expr_list>" parsing.
-- Called when parsing return statements.
-- The specific test for initial ";" is because it's not a block terminator,
-- so without itgg.list would choke on "return ;" statements.
-- We don't make a modified copy of block_terminators because this list
-- is sometimes modified at runtime, and the return parser would get out of
-- sync if it was relying on a copy.
--------------------------------------------------------------------------------
local return_expr_list_parser = gg.multisequence{
   { ";" , builder = function() return { } end }, 
   default = gg.list { 
      expr, separators = ",", terminators = block_terminators } }

		--------------------------------------------------------------------------------
-- for header, between [for] and [do] (exclusive).
-- Return the `Forxxx{...} AST, without the body element (the last one).
--------------------------------------------------------------------------------
function for_header (lx)
   local var = mlp.id (lx)
   if lx:is_keyword (lx:peek(), "=") then 
      -- Fornum: only 1 variable
      lx:next() -- skip "="
      local e = expr_list (lx)
      assert (2 <= #e and #e <= 3, "2 or 3 values in a fornum")
      return { tag="Fornum", var, unpack (e) }
   else
      -- Forin: there might be several vars
      local a = lx:is_keyword (lx:next(), ",", "in")
      if a=="in" then var_list = { var, lineinfo = var.lineinfo } else
         -- several vars; first "," skipped, read other vars
         var_list = gg.list{ 
            primary = id, separators = ",", terminators = "in" } (lx)
         _G.table.insert (var_list, 1, var) -- put back the first variable
         lx:next() -- skip "in"
      end
      local e = expr_list (lx)
      return { tag="Forin", var_list, e }
   end
end

--------------------------------------------------------------------------------
-- Function def parser helper: id ( . id ) *
--------------------------------------------------------------------------------
local function fn_builder (list)
   local r = list[1]
   for i = 2, #list do r = { tag="Index", r, id2string(list[i]) } end
   return r
end
local func_name = gg.list{ id, separators = ".", builder = fn_builder }

--------------------------------------------------------------------------------
-- Function def parser helper: ( : id )?
--------------------------------------------------------------------------------
local method_name = gg.onkeyword{ name = "method invocation", ":", id, 
   transformers = { function(x) return x and id2string(x) end } }

		--------------------------------------------------------------------------------
-- Function def builder
--------------------------------------------------------------------------------
local function funcdef_builder(x)
   local name, method, func = x[1], x[2], x[3]
   if method then 
      name = { tag="Index", name, method, lineinfo = {
         first = name.lineinfo.first,
         last  = method.lineinfo.last } }
      _G.table.insert (func[1], 1, {tag="Id", "self"}) 
   end
   local r = { tag="Set", {name}, {func} } 
   r[1].lineinfo = name.lineinfo
   r[2].lineinfo = func.lineinfo
   return r
end 


--------------------------------------------------------------------------------
-- if statement builder
--------------------------------------------------------------------------------
local function if_builder (x)
   local cb_pairs, else_block, r = x[1], x[2], {tag="If"}
   for i=1,#cb_pairs do r[2*i-1]=cb_pairs[i][1]; r[2*i]=cb_pairs[i][2] end
   if else_block then r[#r+1] = else_block end
   return r
end 

--------------------------------------------------------------------------------
-- produce a list of (expr,block) pairs
--------------------------------------------------------------------------------
local elseifs_parser = gg.list {
   gg.sequence { expr, "then", block },
   separators  = "elseif",
   terminators = { "else", "end" } }

		--------------------------------------------------------------------------------
-- assignments and calls: statements that don't start with a keyword
--------------------------------------------------------------------------------
local function assign_or_call_stat_parser (lx)
   local e = expr_list (lx)
   local a = lx:is_keyword(lx:peek())
   local op = a and stat.assignments[a]
   if op then
      --FIXME: check that [e] is a LHS
      lx:next()
      local v = expr_list (lx)
      if type(op)=="string" then return { tag=op, e, v }
      else return op (e, v) end
   else 
      assert (#e > 0)
      if #e > 1 then 
         gg.parse_error (lx, "comma is not a valid statement separator") end
      if e[1].tag ~= "Call" and e[1].tag ~= "Invoke" then
         gg.parse_error (lx, "This expression is of type '%s'; "..
            "only function and method calls make valid statements", 
            e[1].tag or "<list>")
      end
      return e[1]
   end
end

local_stat_parser = gg.multisequence{
   -- local function <name> <func_val>
   { "function", id, func_val, builder = 
      function(x) 
         local vars = { x[1], lineinfo = x[1].lineinfo }
         local vals = { x[2], lineinfo = x[2].lineinfo }
         return { tag="Localrec", vars, vals } 
      end },
   -- local <id_list> ( = <expr_list> )?
   default = gg.sequence{ id_list, gg.onkeyword{ "=", expr_list },
      builder = function(x) return {tag="Local", x[1], x[2] or { } } end } }

		--------------------------------------------------------------------------------
-- statement
--------------------------------------------------------------------------------
stat = gg.multisequence { 
   name="statement",
   { "do", block, "end", builder = 
      function (x) return { tag="Do", unpack (x[1]) } end },
   { "for", for_header, "do", block, "end", builder = 
      function (x) x[1][#x[1]+1] = x[2]; return x[1] end },
   { "function", func_name, method_name, func_val, builder=funcdef_builder },
   { "while", expr, "do", block, "end", builder = "While" },
   { "repeat", block, "until", expr, builder = "Repeat" },
   { "local", local_stat_parser, builder = fget (1) },
   { "return", return_expr_list_parser, builder = fget (1, "Return") },
   { "break", builder = function() return { tag="Break" } end },
   { "-{", splice_content, "}", builder = fget(1) },
   { "if", elseifs_parser, gg.onkeyword{ "else", block }, "end", 
     builder = if_builder },
   default = assign_or_call_stat_parser }

		stat.assignments = {
   ["="] = "Set" }

		function stat.assignments:add(k, v) self[k] = v end
)###";


std::string mlpext_lua = R"###(
--------------------------------------------------------------------------------
--
-- Non-Lua syntax extensions
--
--------------------------------------------------------------------------------

module ("mlp", package.seeall)

--------------------------------------------------------------------------------
-- Alebraic Datatypes
--------------------------------------------------------------------------------
local function adt (lx)
   local tagval = id (lx) [1]
   local tagkey = {tag="Pair", {tag="String", "tag"}, {tag="String", tagval} }
   if lx:peek().tag == "String" or lx:peek().tag == "Number" then
      return { tag="Table", tagkey, lx:next() }
   elseif lx:is_keyword (lx:peek(), "{") then
      local x = table (lx)
      _G.table.insert (x, 1, tagkey)
      return x
   else return { tag="Table", tagkey } end
end

expr:add{ "`", adt, builder = fget(1) }

--------------------------------------------------------------------------------
-- Anonymous lambda
--------------------------------------------------------------------------------
local lambda_expr = gg.sequence{ 
   "|", func_params_content, "|", expr,
   builder= function (x) 
      local li = x[2].lineinfo
      return { tag="Function", x[1], 
               { {tag="Return", x[2], lineinfo=li }, lineinfo=li } }
   end }

		-- In an earlier version, lambda_expr took an expr_list rather than an expr
-- after the 2nd bar. However, it happened to be much more of a burden than an
-- help, So finally I disabled it. If you want to return several results, 
-- use the long syntax.
--------------------------------------------------------------------------------
-- local lambda_expr = gg.sequence{ 
--    "|", func_params_content, "|", expr_list,
--    builder= function (x) 
--       return {tag="Function", x[1], { {tag="Return", unpack(x[2]) } } } end }

expr:add (lambda_expr)

--------------------------------------------------------------------------------
-- Allows to write "a `f` b" instead of "f(a, b)". Taken from Haskell.
-- This is not part of Lua 5.1 syntax, so it's added to the expression
-- afterwards, so that it's easier to disable.
--------------------------------------------------------------------------------
local function expr_in_backquotes (lx) return expr(lx, 35) end

expr.infix:add{ name = "infix function", 
   "`", expr_in_backquotes, "`", prec = 35, assoc="left", 
   builder = function(a, op, b) return {tag="Call", op[1], a, b} end }


		--------------------------------------------------------------------------------
-- table.override assignment
--------------------------------------------------------------------------------

mlp.lexer:add "<-"
stat.assignments["<-"] = function (a, b)
   assert( #a==1 and #b==1, "No multi-args for '<-'")                         
   return { tag="Call", { tag="Index", { tag="Id", "table" },
                                       { tag="String", "override" } },
                        a[1], b[1]} 
end

--------------------------------------------------------------------------------
-- C-style op+assignments
--------------------------------------------------------------------------------
local function op_assign(kw, op) 
   local function rhs(a, b)
      return { tag="Op", op, a, b } 
   end
   local function f(a,b) 
      return { tag="Set", a, _G.table.imap(rhs, a, b) }
   end
   mlp.lexer:add (kw)
   mlp.stat.assignments[kw] = f
end

_G.table.iforeach (op_assign, 
                {"+=", "-=", "*=", "/="},
                {"add", "sub", "mul", "div"})
)###";

std::string ast2cast_lua = R"###(
--
-- ast2cast.lua
-- Converts Lua ASTs to C ASTs.
--
-- Variable naming conventions: ast (Lua AST), cast (C AST).
--
-- (c) 2008 David Manura.  Licensed in the same terms as Lua (MIT license).
-- See included LICENSE file for full licensing details.

--luaanalyze checks
--! local t_G = typeimport"luaanalyze.library.standard"
--! t_G.add_field "mlc"
--! t_G.add_field "mlp"
--! local ast_t = require "luaanalyze.type.ast"
--! ast_t.add_field "name"
--! ast_t.add_field "upvalue"
--! ast_t.add_field "idx"
--! ast_t.add_field "type"
--! typematch("[^c]ast$", ast_t) --FIX:match
--! checkglobals()
--! checktypes()

_G.M = {}

local _G           = _G
local assert       = _G.assert
local ipairs       = _G.ipairs
local math         = _G.math
local select       = _G.select
local setmetatable = _G.setmetatable
local string       = _G.string
local tostring     = _G.tostring
local type         = _G.type
local table        = _G.table
local unpack       = _G.unpack


-- converts Metalua AST <ast> built from string
-- <src> to C AST <cast>.
-- returns(cast)
local function ast_to_cast(src, ast)
--##------------------------------------------------------------------
--## Note: this is a large function nesting many closures;
--## indentation of its contents is omitted.
--##------------------------------------------------------------------


-- info on current scope. includes
-- table of local variables.
-- Maps name -> stack index
local _currentscope = {['.closurelevel'] = 0}

-- topmost stack index
local _idxtop = 0

local _names = {}

local _varid = 0

-- note: _is_created maps function name to boolean indicated whether
-- function has been generated.
local _is_created = {}


-- Information on function currently being compiled.
local _funcinfo = {is_vararg=false, nformalparams=0,
  is_lc_nextra_used=false, is_lc_nactualargs_used=false,
  is_lc_nextra_used_debug=false, is_lc_nactualargs_used_debug=false,
  idxtopmax = 0
  --old: outervars={}
}


-- LUA_MINSTACK value in luaconf.h
local LUA_MINSTACK = 20


-- Mutators for _idxtop.
local function idxtop_change(n)
  _idxtop = _idxtop + n
  _funcinfo.idxtopmax = math.max(_funcinfo.idxtopmax, _idxtop)
end
local function idxtop_restore(idx)
  assert(idx <= _idxtop)
  _idxtop = idx
end

-- Builds set from elements in array t.
local function makeset(t)
  local set = {}
  for _,v in ipairs(t) do set[v] = true end
  return set
end


-- Set of identifiers of binary operators
-- (metamethod name without "__" prefix).
local is_binopid = makeset {
  "add",   "sub", "mul",    "div",
  "mod",   "pow", "concat", "eq",
  "lt",    "le",  "and",    "or"
}

-- Set of identifiers of unary operators.
local is_unopid = makeset {
  "not", "len", "unm"
}

-- Set of binary ops with metamethod behavior like "+".
local is_arithbinop = makeset {
  "add", "sub", "mul", "div", "mod", "pow"
}

-- Maps operator identifier to function
-- implementing that identifier (without metamethods).
local fops = {
  ["add"]   =function(a,b) return a+b end,
  ["sub"]   =function(a,b) return a-b end,
  ["mul"]   =function(a,b) return a*b end,
  ["div"]   =function(a,b) return a/b end,
  ["mod"]   =function(a,b) return a%b end,
  ["pow"]   =function(a,b) return a^b end,
  ["concat"]=function(a,b) return a..b end,
  ["eq"]    =function(a,b) return a==b end,
  ["lt"]    =function(a,b) return a<b end,
  ["le"]    =function(a,b) return a<=b end,
  ["and"]   =function(a,b) return a and b end,
  ["or"]    =function(a,b) return a or b end,
  ["not"]   =function(a)   return not a end,
  ["len"]   =function(a)   return #a end,
  ["unm"]   =function(a)   return -a end
}

-- Maps operator identifier to function that returns
-- a C code string implementing that identifier
-- (without metamethods).
-- Only for binary arthmetic ops.
local opid_to_c = {
  ["add"]=function(a,b) return a .. ' + ' .. b end,
  ["sub"]=function(a,b) return a .. ' - ' .. b end,
  ["mul"]=function(a,b) return a .. ' * ' .. b end,
  ["div"]=function(a,b) return a .. ' / ' .. b end,
  ["mod"]=function(a,b) return
            a .. ' - floor(' .. a .. '/' .. b ..')*' .. b end,
          -- caution: a and b evaluate twice
          -- note: requies math.h
  ["pow"]=function(a,b) return 'pow(' .. a .. ',' .. b .. ')' end,
          -- note: requies math.h
}

-- Converts Lua object to Lua AST.
local function obj_to_ast(obj)
  if obj == nil then
    return {tag='Nil'}
  elseif obj == true then
    return {tag='True'}
  elseif obj == false then
    return {tag='False'}
  elseif type(obj) == 'number' then
    return {tag='Number', obj}
  elseif type(obj) == 'string' then
    return {tag='String', obj}
  else
    assert(false, tostring(obj))
  end
end

-- remove whitespace from front and end of string
local function trim(s)
  return s:gsub('^%s+', ''):gsub('%s+$', '')
end

-- Prepends comment to comments above C AST node.
local function prepend_comment(cast, comment)
  local s = cast.comment and comment .. '\n' .. cast.comment or comment
  cast.comment = s
end

-- Appends comment to comments above C AST node.
local function append_comment(cast, comment)
  local s = cast.comment and cast.comment .. '\n' .. comment or comment
  cast.comment = s
end

-- Appends comment to comments after C AST node.
local function append_comment_below(cast, comment)
  local s = cast.aftercomment and cast.aftercomment .. '\n' ..
            comment or comment
  cast.aftercomment = s
end


-- Appends elements to array t2 to array t1.
local function append_array(t1,t2)
  for _,v in ipairs(t2) do t1[#t1+1] = v end
end

local function append_cast(cast1, cast2)
  if cast2.tag ~= nil then -- enclosing block omitted (convenience)
                           -- :IMPROVE: the convenience possibly isn't
                           -- worth the ambiguity. make separate function?
    assert(not cast2.pre)
    assert(not cast2.idx)
    cast2 = {cast2, pre=cast2.pre or {}, idx=nil}
  end
  append_array(cast1, cast2)
  if cast2.comment then
    if #cast2 > 0 then
      prepend_comment(cast2[1], cast2.comment)
    elseif #cast1 > 0 then
      append_comment_below(cast1[#cast1], cast2.comment)
    else
      assert(false)
    end
  end
  if cast2.aftercomment then
    if #cast2 > 0 then
      append_comment_below(cast2[#cast2], cast2.aftercomment)
    elseif #cast1 > 0 then
      append_comment_below(cast1[#cast1], cast2.aftercomment)
    else
      assert(false)
    end
  end
  append_array(cast1.pre, cast2.pre)
  return cast2  --FIX:improve style?
end


-- Constructor and type for C AST nodes.
local cexpr_mt = {}
cexpr_mt.__index = cexpr_mt
function cexpr_mt:append(cast)
  return append_cast(self, cast)
end
local function cexpr(...)
  return setmetatable({idx=-1, pre={}, ...}, cexpr_mt)
end

local C_mt = {}
function C_mt.__index(t,k,v)
  local f = function(...) return {tag=k, ...} end
  t.k = f -- cache
  return f
end
local C = setmetatable({}, C_mt)

local function tag(o)
  return type(o) == 'table' and o.tag or nil
end

-- returns C AST of double for given C AST idx.
local function todouble(idx)
  if type(idx) == 'string' then
    return C.Id(idx)
  else
    return C.lua_tonumber(idx)
  end
end

-- returns C AST of bool for given C AST idx.
local function tobool(idx)
  if type(idx) == 'string' then
    return C.Id(idx)
  else
    return C.lua_toboolean(idx)
  end
end


-- Adjusts absolute index of local variable,
-- given that in vararg (...) functions, base index depends on
-- arguments passed in.  Can return C AST.
local function realidx(idx)
  -- pseudoindex
  if type(idx) == 'table' and idx.tag == 'C' and
     idx[1] == 'lua_upvalueindex(1)'
  then
    return idx
  end
  local idxreal = tag(idx) == 'Upval' and idx[1] or idx
  if _funcinfo.is_vararg and
     (tag(idxreal) == 'Id' or idxreal > _funcinfo.nformalparams)
  then
    if tag(idx) == 'Upval' then
      -- no adjustment
    else
      idx = C.Op('+', idx, C.Id'lc_nextra')
      _funcinfo.is_lc_nextra_used = true
    end
  end
  return idx
end

-- Gets real index of local var.
local function localidx(name)
  local idx = _currentscope[name]
  if not idx then return end

		  return realidx(idx)
end


--old: local function isupvalue(name)
--  return not _currentscope[name] and _funcinfo.outervars[name]
--end

-- Adjusts stack index to relative (negative) position offset.  note:
-- use only when values inside offset are on the stack
-- (non-psuedoindices).
-- Note: allows C AST.
local function adjustidx(offset, idx)
  if type(idx) ~= 'number' or idx > 0 then
    return idx
  else -- relative stack index or pseudoindex
     -- FIX:pseudoindices not supported?
    return idx + offset + 1
  end
end

-- Adjusts relative stack indicies
-- Note: allows C ASTs.
local function adjustidxs(...)
  local nused = 0
  local result = {...}
  for i=#result,1,-1 do
    local idx = result[i]
    if type(idx) == 'number' and idx < 0 then
            --FIX: and idx > LUA_REGISTRYINDEX ?
      result[i] = result[i] - nused   -- adjust
    end
    if idx == -1 then
      nused = nused + 1
    end
  end
  return unpack(result)
end

-- Counts number of temporary values on stack.
local function countstack(...)
  local n = select('#', ...)
  local ncount = 0
  for i=1,n do
    local idx = select(i, ...)
    if idx == -1 then ncount = ncount + 1 end
  end
  return ncount
end

-- Given AST ast taken from inside code string src,
-- return code string representing AST.
local function get_ast_string(src, ast, ...)
  local first = ast.lineinfo.first[3]
  local last  = ast.lineinfo.last [3]
  local code = src:sub(first, last)
  return code
end

-- Gets next unique ID.  Useful for generating unique variable names.
-- orig_name is string of arbitrary text to base variable name
--   on (optional and may be ignored)
-- prefix is prefix string for variable (defaults to '')
local MAX_IDENTIFIER_LENGTH = 60 -- note: not a hard limit
local function nextid(orig_name, prefix)
  orig_name = orig_name or ''
  prefix = prefix or ''

		  -- ensure will form a valid C identifier
  local name = orig_name:gsub('[^%w_]', '_')

		  -- ensure uniqueness
  _names[name] = (_names[name] or 0) + 1

		  local id =
    'lc' .. prefix .. _names[name] .. (name == '' and '' or '_') .. name
  return id
end

-- Gets next unique ID for lexical variable.
local function nextvarid()
  _varid = _varid + 1
  return _varid
end

local function newscope()
  local currentscope_old = _currentscope
  _currentscope = setmetatable({}, {__index = currentscope_old})
  return currentscope_old
end

local function restore_scope(currentscope_save)
  _currentscope = currentscope_save
end

local function restore_stack_rel(cast, idx_old)
  local npushed = _idxtop - idx_old
  if npushed ~= 0 then
    cast:append(C.lua_pop(npushed))
    append_comment(cast[#cast], 'internal: stack cleanup on scope exit')
  end
  idxtop_change(- npushed)
end

local function restore_stack_abs(cast, idx_old_cast, idx_old)
  cast:append(C.lua_settop(realidx(idx_old_cast)))
  idxtop_restore(idx_old)
end


-- Returns whether expression could possibly return a number of argument
-- different from 1.
-- note: expr_ast can be nil
local function can_multi(expr_ast)
  return expr_ast and (expr_ast.tag == 'Call' or expr_ast.tag == 'Invoke'
                       or expr_ast.tag == 'Dots')
end


-- Performs constant folding on AST.
local function constant_fold(ast)
  if ast.tag == 'Op' then
    local opid = ast[1]
    local a_ast = constant_fold(ast[2])
    local b_ast = ast[3] and constant_fold(ast[3])
    if b_ast then  -- binary op
      if a_ast.tag == 'Number' and b_ast.tag == 'Number' then
        return obj_to_ast(fops[opid](a_ast[1], b_ast[1]))
      end
    else  -- unary op
      if a_ast.tag == 'Number' then
        return obj_to_ast(fops[opid](a_ast[1]))
      end
    end
  end
  return ast
end


local function get_closuretableidx_cast()
  return localidx('.closuretable')
end

-- Deduce names of functions by the variables they are assigned to.
-- Given lists of LHS and RHS expressions, adds a "name" field
-- to any applicable RHS expression.
local function deduce_function_names(ls_ast, rs_ast)
  for i,r_ast in ipairs(rs_ast) do
    local l_ast = ls_ast[i]
    if ls_ast[i] and r_ast.tag == 'Function' then
      local name = get_ast_string(src, l_ast)
      r_ast.name = name
    end
  end
end


-- forward declarations
local genexpr
local genblock

-- Converts any Lua arithmetic op AST to C AST.
local function genarithbinop(ast, where)
  -- Returns C AST node definition arithmetic binary op with given
  -- identitifier.
  local function makebinop(opid)
    local ccode = string.format([[
/* __%s metamethod handler.
 * warning: assumes indices in range LUA_REGISTRYINDEX < x < 0 are relative. */
static void lc_%s(lua_State * L, int idxa, int idxb) {
  if (lua_isnumber(L,idxa) && lua_isnumber(L,idxb)) {
    lua_pushnumber(L,%s);
  }
  else {
    if (luaL_getmetafield(L,idxa,"__%s")||luaL_getmetafield(L,idxb,"__%s")) {
      lua_pushvalue(L,idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa-1 : idxa);
      lua_pushvalue(L,idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb-2 : idxb);
      lua_call(L,2,1);
    }
    else {
      luaL_error(L, "attempt to perform arithmetic");
    }
  }
}
]],   opid, opid,
      assert(opid_to_c[opid])("lua_tonumber(L,idxa)", "lua_tonumber(L,idxb)"),
      opid, opid)

		    return C.C(ccode)
  end

		  local opid, a_ast, b_ast = ast[1], ast[2], ast[3]

		  local cast = cexpr()

		  if opid == 'pow' or opid == 'mod' then
    if not _is_created['math.h'] then
      append_array(cast.pre, {C.Include'<math.h>'})
      _is_created['math.h'] = true
    end
  end

		  local fname = "lc_" .. opid
  if not _is_created[fname] then
    append_array(cast.pre, {makebinop(opid)})
    _is_created[fname] = true
  end

		  local a_idx = cast:append(genexpr(a_ast, 'anywhere')).idx
  local b_idx = cast:append(genexpr(b_ast, 'anywhere')).idx
  local nstack = countstack(a_idx, b_idx)
  a_idx, b_idx = adjustidxs(a_idx, b_idx)
  cast:append(C.CallL('lc_'..opid, a_idx, b_idx))
  for i=1,nstack do
    cast:append(C.lua_remove(-2))
  end

		  return cast
end

-- Converts Lua equality op AST to C AST.
local function geneqop(ast, where)
  local a_ast, b_ast = ast[2], ast[3]
  local cast = cexpr()
  local a_idx = cast:append(genexpr(a_ast, where)).idx
  local b_idx = cast:append(genexpr(b_ast, where)).idx
  local nstack = (a_idx == -1 and 1 or 0) + (b_idx == -1 and 1 or 0)
  a_idx, b_idx = adjustidxs(a_idx, b_idx)
  local id = nextid(); cast:append(C.Let(id, C.lua_equal(a_idx, b_idx)))
  if nstack ~= 0 then
    cast:append(C.lua_pop(nstack))
  end
  cast:append(C.lua_pushboolean(C.Id(id)))
  return cast
end

-- Converts Lua less-than op AST to C AST.
local function genltop(ast, where)
  local opid, a_ast, b_ast = ast[1], ast[2], ast[3]
  local cast = cexpr()

		  local a_idx = cast:append(genexpr(a_ast, 'anywhere')).idx
  local b_idx = cast:append(genexpr(b_ast, 'anywhere')).idx
  local nstack = countstack(a_idx, b_idx)
  a_idx, b_idx = adjustidxs(a_idx, b_idx)

		  local id = nextid(); cast:append(C.Let(id, C.lua_lessthan(a_idx, b_idx)))

		  if nstack ~= 0 then
    cast:append(C.lua_pop(nstack))
  end

		  cast:append(C.lua_pushboolean(C.Id(id)))

		  return cast
end

)###"
R"###(

-- Converts Lua less-than-or-equal op AST to C AST.
-- Why does the Lua C API implement lua_lessthan but not lua_lessequal?
-- (note: lessqual is defined in lvm.c).
local function genleop(ast, where)
  local function makeop()
    local cast = C.C [[
/* warning: assumes indices in range LUA_REGISTRYINDEX < x < 0 are relative. */
static int lc_le(lua_State * L, int idxa, int idxb) {
  if (lua_type(L,idxa) == LUA_TNUMBER && lua_type(L,idxb) == LUA_TNUMBER) {
    return lua_tonumber(L,idxa) <= lua_tonumber(L,idxb);
  }
  else if (lua_type(L,idxa) == LUA_TSTRING && lua_type(L,idxb) == LUA_TSTRING) {
    /* result similar to lvm.c l_strcmp */
    return lua_lessthan(L,idxa,idxb) || lua_rawequal(L,idxa,idxb);
  }
  else if (luaL_getmetafield(L,idxa,"__le")||luaL_getmetafield(L,idxb,"__le")) {
    lua_pushvalue(L,idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa-1 : idxa);
    lua_pushvalue(L,idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb-2 : idxb);
    lua_call(L,2,1);
    const int result = lua_toboolean(L,-1);
    lua_pop(L,1);
    return result;
  }
  else if (luaL_getmetafield(L,idxa,"__lt")||luaL_getmetafield(L,idxb,"__lt")) {
    lua_pushvalue(L,idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb-1 : idxb);
    lua_pushvalue(L,idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa-2 : idxa);
    lua_call(L,2,1);
    const int result = ! lua_toboolean(L,-1);
    lua_pop(L,1);
    return result;
  }
  else {
    return luaL_error(L, "attempt to compare");
  }
}
]]

    return cast
  end

		  local opid, a_ast, b_ast = ast[1], ast[2], ast[3]
  local cast = cexpr()

		  local fname = "lc_le"
  if not _is_created[fname] then
    append_array(cast.pre, {makeop(opid)})
    _is_created[fname] = true
  end

		  local a_idx = cast:append(genexpr(a_ast, 'anywhere')).idx
  local b_idx = cast:append(genexpr(b_ast, 'anywhere')).idx
  local nstack = countstack(a_idx, b_idx)
  a_idx, b_idx = adjustidxs(a_idx, b_idx)
  if nstack == 0 then
    cast:append(C.lua_pushboolean(C.CallL('lc_le', a_idx, b_idx)))
  else
    local id = nextid()
    cast:append(C.Let(id, C.CallL('lc_le', a_idx, b_idx)))
    cast:append(C.lua_pop(nstack))
    cast:append(C.lua_pushboolean(C.Id(id)))
  end

		  return cast
end

-- Converts Lua binary logical op AST to C AST.
local function genlogbinop(ast, where)
  local opid, a_ast, b_ast = ast[1], ast[2], ast[3]
  local cast = cexpr()

		  cast:append(genexpr(a_ast, 'onstack'))

		  assert(opid == 'and' or opid == 'or')
  local expr_cast = C.lua_toboolean(-1)
  if opid == 'or' then expr_cast = C.Not(expr_cast) end
  local block_cast = cexpr()
  block_cast:append(C.lua_pop(1))
  block_cast:append(genexpr(b_ast, 'onstack'))
  cast:append(C.If(expr_cast, block_cast))
  append_array(cast.pre, block_cast.pre)

		  return cast
end

-- Converts Lua unary logical (i.e. not) op AST to C AST.
local function gennotop(ast, where)
  local opid, a_ast = ast[1], ast[2]
  local cast = cexpr()

		  local a_idx = cast:append(genexpr(a_ast, 'anywhere')).idx

		  cast:append(C.lua_pushboolean(C.Not(tobool(a_idx))))
  if a_idx == -1 then
    cast:append(C.lua_remove(-2))
  end

		  return cast
end

-- Converts Lua length op AST to C AST.
local function genlenop(ast, where)
  local opid, a_ast = ast[1], ast[2]

		  local cast = cexpr()
  local a_idx = cast:append(genexpr(a_ast, 'anywhere')).idx

		  --FIX:call metatable __len for userdata?
  local id = nextid()
  cast:append(C.LetDouble(id, C.lua_objlen(a_idx)))
  if a_idx == -1 then
    cast:append(C.lua_pop(1))
  end
  cast:append(C.lua_pushnumber(C.Id(id)))
  return cast
end


-- Converts Lua unary minus op AST to C AST.
local function genunmop(ast, where)
  -- Returns C AST node definition of op.
  local function makeop()
    local ccode = [[
/* __unm metamethod handler.
 * warning: assumes indices in range LUA_REGISTRYINDEX < x < 0 are relative. */
static void lc_unm(lua_State * L, int idxa) {
  if (lua_isnumber(L,idxa)) {
    lua_pushnumber(L,- lua_tonumber(L, idxa));
  }
  else {
    if (luaL_getmetafield(L,idxa,"__unm")) {
      lua_pushvalue(L,idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa-1 : idxa);
      lua_call(L,1,1);
    }
    else {
      luaL_error(L, "attempt to perform arithmetic");
    }
  }
}
]]
    return C.C(ccode)
  end

		  local opid, a_ast = ast[1], ast[2]
  assert(opid == 'unm')

		  local cast = cexpr()

		  local fname = "lc_" .. opid
  if not _is_created[fname] then
    append_array(cast.pre, {makeop()})
    _is_created[fname] = true
  end

		  local a_idx = cast:append(genexpr(a_ast, 'anywhere')).idx
  local nstack = countstack(a_idx)
  a_idx = adjustidxs(a_idx)
  cast:append(C.CallL('lc_'..opid, a_idx))
  for i=1,nstack do
    cast:append(C.lua_remove(-2))
  end

		  return cast
end

-- Converts Lua concatenation op AST to C AST.
local function genconcatop(ast, where)
  local a_ast, b_ast = ast[2], ast[3]
  local cast = cexpr()

		  cast:append(genexpr(a_ast, 'onstack'))
  cast:append(genexpr(b_ast, 'onstack'))
  cast:append(C.lua_concat(2))

		  return cast
end

-- Creates C AST for table used to manage upvalues
-- for a closure.
-- note: call activate_closure_table() afterward.
local function gennewclosuretable()
  local cast = cexpr()

		  if not _is_created['lc_newclosuretable'] then
    -- note: index idx cannot be relative.
    -- note: key 0 points to parent table.
    local body_cast = cexpr()
    --IMPROVE: C.C usage
    body_cast:append(C.C[[
/* pushes new closure table onto the stack, using closure table at
 * given index as its parent */
static void lc_newclosuretable(lua_State * L, int idx) {]])

    body_cast:append( cexpr( cexpr(
      C.lua_newtable(),
      C.lua_pushvalue(C.Id'idx'),
      C.lua_rawseti(-2,0)
    )))
    body_cast:append(C.C'}')

		    append_array(cast.pre, body_cast)

		    _is_created['lc_newclosuretable'] = true
  end

		  cast:append(C.CallL('lc_newclosuretable', get_closuretableidx_cast()))
  idxtop_change(1)

		  local id = nextid()
  cast:append(C.Enum(id, _idxtop))
  if not _is_created['assert.h'] then
    append_array(cast.pre, {C.Include'<assert.h>'})
    _is_created['assert.h'] = true
  end
  cast:append(C.Call('assert', C.Op('==', C.lua_gettop(), realidx(C.Id(id)))))

		  cast.idx = C.Id(id)

		  return cast
end

local function activate_closure_table(idx)
  _currentscope['.closuretable'] = idx
  _currentscope['.closurelevel'] = _currentscope['.closurelevel'] + 1
end

-- Convert Lua `Function AST to C AST.
-- C AST defines the function's implementation.
-- helper function to generate function definition
-- is_main - whether this is the top-level "main" function.
local function genfunctiondef(ast, ismain)
  local params_ast, body_ast = ast[1], ast[2]

		  -- localize new function info.
  local funcinfo_old = _funcinfo
  local has_vararg = params_ast[#params_ast]
                     and params_ast[#params_ast].tag == 'Dots'
  local nformalargs = #params_ast - (has_vararg and 1 or 0)
  _funcinfo = {is_vararg = has_vararg, nformalparams = nformalargs,
    is_lc_nextra_used = false, is_lc_nactualargs_used=false,
    is_lc_nextra_used_debug = false, is_lc_nactualargs_used_debug=false,
    idxtopmax = 0
    --old: outervars = _currentscope
  }

		  -- note: special usage of _idxtop
  local idxtop_old = _idxtop
  _idxtop = 0

		  -- localize code
  local currentscope_save = newscope()
  _currentscope['.closuretable'] = C.C'lua_upvalueindex(1)'

		  -- define parameters as local variables.
  for i,var_ast in ipairs(params_ast) do
    if var_ast.tag ~= 'Dots' then
      assert(var_ast.tag == 'Id')
      local varname = var_ast[1]
      _currentscope[varname] = i
      idxtop_change(1)
    end
  end

		  local body_cast = cexpr()

		  local is_upvalue = false
  for i,params_ast in ipairs(params_ast) do
    is_upvalue = is_upvalue or params_ast.upvalue
  end
  if is_upvalue then
    local ct_idx = body_cast:append(gennewclosuretable()).idx
    activate_closure_table(ct_idx)

		    for i=1,nformalargs do
      local param_ast = params_ast[i]

		      if param_ast.upvalue then
        local name = param_ast[1]
        local varid = nextvarid()

		        -- create local symbol
        _currentscope[name] =
          {tag='Upval', _idxtop, _currentscope['.closurelevel'], varid}

		        body_cast:append(C.lua_pushvalue(i))
        body_cast:append(C.lua_rawseti(-2, varid))
      end
    end
  end

		  -- generate body
  body_cast:append(genblock(body_ast))

		  local bodypre_cast = cexpr()

		  -- Ensure stack space.
  local fudge = 10 -- allow some extra space, avoiding detailed
                   -- accounting. FIX: is this always sufficient?
  if _funcinfo.idxtopmax + fudge >= LUA_MINSTACK then
    bodypre_cast:append(C.lua_checkstack(_funcinfo.idxtopmax + fudge))
  end

		  -- note: do after generating body do that _funcinfo params are set
  -- note: safety in case caller passes more or less arguments
  -- than parameters.  IMPROVE-some of this is sometimes not necessary.
  
  bodypre_cast:append(C.LetInt('lc_nformalargs', nformalargs))
  if #params_ast > 0 and params_ast[#params_ast].tag == 'Dots' or
     ismain  -- dots implicit in main chunk
  then
    if nformalargs > 0 then
      bodypre_cast:append(
        C.If(C.Op('<', C.lua_gettop(), C.Id'lc_nformalargs'),
          { C.lua_settop(C.Id'lc_nformalargs') } ))
    end
    -- note: measure nactualargs after adjustment above
    -- (important for genexpr of `Id)
    if _funcinfo.is_lc_nextra_used then
      _funcinfo.is_lc_nactualargs_used = true
    elseif _funcinfo.is_lc_nextra_used_debug then
      _funcinfo.is_lc_nactualargs_used_debug = true
    end

		    if _funcinfo.is_lc_nactualargs_used then
      bodypre_cast:append(C.LetInt('lc_nactualargs', C.lua_gettop()))
    elseif _funcinfo.is_lc_nactualargs_used_debug then
      --bodypre_cast:append(C.C'#ifndef NDEBUG')
      bodypre_cast:append(C.LetInt('lc_nactualargs', C.lua_gettop()))
      --bodypre_cast:append(C.C'#endif')
    end
    if _funcinfo.is_lc_nextra_used then
      bodypre_cast:append(C.LetInt('lc_nextra',
        C.Op('-', C.Id'lc_nactualargs', C.Id'lc_nformalargs')))
    elseif _funcinfo.is_lc_nextra_used_debug then
      --bodypre_cast:append(C.C'#ifndef NDEBUG')
      bodypre_cast:append(C.LetInt('lc_nextra',
        C.Op('-', C.Id'lc_nactualargs', C.Id'lc_nformalargs')))
      --bodypre_cast:append(C.C'#endif')
    end
  else
    bodypre_cast:append(C.lua_settop(#params_ast))
  end
 
  -- prepend bodypre_cast to body_cast (IMPROVE: add prepend method?)
  local body_old_cast = body_cast
  local body_cast = cexpr()
  body_cast:append(bodypre_cast)
  body_cast:append(body_old_cast)

		  if #body_ast == 0 or body_ast[#body_ast].tag ~= 'Return' then
    body_cast:append(C.Return(0))
  end

		  local id = ast.is_main and 'lcf_main'
             or nextid(ast.name, 'f')
  local def_cast = cexpr()
  append_array(def_cast, body_cast.pre)
  local func_cast = C.Functiondef(id, body_cast)

		  local comment =
    ast.is_main and '(...)' or
    trim(src:sub(ast.lineinfo.first[3], params_ast.lineinfo.last[3])) .. ')'
  if comment:sub(1,1) == '(' then
    comment = 'function' .. comment
  end
  if ast.name then
    comment = 'name: ' .. ast.name .. '\n' .. comment
  end
  --func_cast.comment = comment

		  func_cast.id = id
  append_array(def_cast, { func_cast })

		  restore_scope(currentscope_save)
  _idxtop = idxtop_old

		  _funcinfo = funcinfo_old

		  return def_cast
end

-- Converts Lua `Function AST to C AST.
-- The C AST instantiates the function/closure (and includes code to define
-- the function implementation also).
local function genfunction(ast, where)
  local cast = cexpr()

		  --## generate function definition
  local def_cast = genfunctiondef(ast)
  append_array(cast.pre, def_cast)

		  --## generate function object
  if ast.uses_upvalue then
    cast:append(C.lua_pushvalue(get_closuretableidx_cast()))
    cast:append(C.lua_pushcclosure(C.Id(def_cast[#def_cast].id), 1))
  else
    cast:append(C.lua_pushcfunction(C.Id(def_cast[#def_cast].id)))
  end

		  return cast
end

-- Convert Lua top-level block AST (for top-level main function)
-- to C AST.
local function genmainchunk(ast)
  local astnew = {tag='Function', is_main=true, name='(main)', {{tag='Dots'}},
    ast, lineinfo=ast.lineinfo} -- note: lineinfo not cloned. ok?

		  return genfunctiondef(astnew, true)
end


-- Converts Lua `Table AST to C AST.
local function gentable(ast, where)
  local cast = cexpr()
  local narr = 0
  local nrec = 0
  for _,e_ast in ipairs(ast) do
    if e_ast.tag == 'Pair' then    -- Q: always true?
      nrec = nrec + 1
    else
      narr = narr + 1
    end
  end
  if narr + nrec == 0 then
    cast:append(C.lua_newtable())
  else
    cast:append(C.lua_createtable(narr,nrec))
  end

		  local pos = 0
  for i,e_ast in ipairs(ast) do
    if e_ast.tag == 'Pair' then
      local k_ast, v_ast = e_ast[1], e_ast[2]
      cast:append(genexpr(k_ast, 'onstack'))
      cast:append(genexpr(v_ast, 'onstack'))
      cast:append(C.lua_rawset(-3))
    else
      local can_multret = i == #ast and can_multi(e_ast)

		      if can_multret then
        -- table marker
        local id = nextid()
        cast:append(C.Let(id, C.lua_gettop()))

		        -- get list of values
        cast:append(genexpr(e_ast, 'onstack', 'multret'))

		        -- push list of values in table.
        -- note: Lua spec does not prohibit right-to-left insertion.
        -- IMPROVE? right-to-left insertion can cause needless placement
        -- of values into the hash part of the table.
        cast:append(
          C.While(C.Op('>', C.lua_gettop(), C.Id(id)), {
            C.lua_rawseti(C.Id(id),
              C.Op('+', pos, C.Op('-', C.lua_gettop(), C.Id(id)))) } ))
      else
        cast:append(genexpr(e_ast, 'onstack'))
        pos = pos + 1
        cast:append(C.lua_rawseti(-2, pos))
      end
    end
  end
  return cast
end

-- Converts Lua `Call or `Invoke AST to C AST.
local function gencall(ast, where, nret)
  local isinvoke = ast.tag == 'Invoke'  -- method call
  local cast = cexpr()

		  -- whether last argument might possibly return multiple values
  local can_multret = can_multi(ast[#ast])

		  -- push function
  if isinvoke then
    local obj_ast = ast[1]
    cast:append(genexpr(obj_ast, 'onstack')) -- actually self
  else
    assert(ast.tag == 'Call')
    local f_ast = ast[1]
    cast:append(genexpr(f_ast, 'onstack'))
  end

		  -- argument marker
  local id
  if can_multret then
    id = nextid(); cast:append(C.Let(id, C.lua_gettop()))
  end

		  -- push arguments
  for i=2,#ast do
    if i == 2 and isinvoke then
      local methodname_ast = ast[i]
      cast:append(genexpr(methodname_ast, 'onstack'))
      cast:append(C.lua_gettable(-2))
      cast:append(C.lua_insert(-2))  -- swap self and function
    else
      cast:append(genexpr(ast[i], 'onstack', i == #ast and 'multret' or false))
    end
  end
  local narg = can_multret and C.Op('-', C.lua_gettop(), C.Id(id)) or #ast-1

		  -- call
  cast:append(C.lua_call(narg,
    nret == 'multret' and C.C'LUA_MULTRET' or nret or 1))

		  return cast
end


-- helper
local function gen_lc_setupvalue(...)
  local cast = cexpr(C.CallL('lc_setupvalue', ...))
  if not _is_created['lc_setupvalue'] then
    append_array(cast.pre, {C.C[[
static void lc_setupvalue(lua_State * L, int tidx, int level, int varid) {
  if (level == 0) {
    lua_rawseti(L,tidx,varid);
  }
  else {
    lua_pushvalue(L,tidx);
    while(--level >= 0) {
      lua_rawgeti(L,tidx,0); /* 0 links to parent table */
      lua_remove(L,-2);
      tidx = -1;
    }
    lua_insert(L,-2);
    lua_rawseti(L,-2,varid);
    lua_pop(L,1);
  }
}
]]})
    _is_created['lc_setupvalue'] = true
  end
  return cast
end

)###"
R"###(

-- helper
local function gen_lc_getupvalue(tidx, level, varid)
  assert(tidx and level and varid)
  local cast = cexpr(C.CallL('lc_getupvalue', tidx, level, varid))
  if not _is_created['lc_getupvalue'] then
    append_array(cast.pre, {C.C[[
/* gets upvalue with ID varid by consulting upvalue table at index
 * tidx for the upvalue table at given nesting level. */
static void lc_getupvalue(lua_State * L, int tidx, int level, int varid) {
  if (level == 0) {
    lua_rawgeti(L,tidx,varid);
  }
  else {
    lua_pushvalue(L,tidx);
    while (--level >= 0) {
      lua_rawgeti(L,tidx,0); /* 0 links to parent table */
      lua_remove(L,-2);
      tidx = -1;
    }
    lua_rawgeti(L,-1,varid);
    lua_remove(L,-2);
  }
}
]]})
    _is_created['lc_getupvalue'] = true
  end
  return cast
end

-- Converts Lua `Number AST to C C AST
local function gennumber(x, pre_cast)
  if x == math.huge or x == -math.huge then
    if not _is_created['math.h'] then
      append_array(pre_cast, {C.Include'<math.h>'})
      _is_created['math.h'] = true
    end
  end
  return x
end

)###"
R"###(

-- Converts any Lua expression AST to C AST.
-- (local)
function genexpr(ast, where, nret)
  if ast.tag == 'Op' then
    ast = constant_fold(ast)
  end

		  if ast.tag == 'Nil' then
    return cexpr(C.lua_pushnil())
  elseif ast.tag == 'Dots' then
    if nret == 'multret' then
      _funcinfo.is_lc_nactualargs_used = true
      return cexpr(C.C[[{int i; for (i=lc_nformalargs+1; i<=lc_nactualargs; i++) { lua_pushvalue(L, i); }}]])
    elseif nret and nret > 1 then
      _funcinfo.is_lc_nextra_used = true
      return cexpr(C.C([[
/* ... */
{ int i;
  const int idx = lua_gettop(L);
  const int npush = (]] .. nret .. [[ <= lc_nextra) ? ]] .. nret .. [[ : lc_nextra;
  for (i=lc_nformalargs+1; i<=lc_nformalargs+npush; i++) { lua_pushvalue(L, i); }
  lua_settop(L, idx + ]] .. nret .. [[); }]]))
    else
      return cexpr(C.lua_pushvalue(C.Op('+', C.Id'lc_nformalargs', 1)));
    end
  elseif ast.tag == 'True' then
    return cexpr(C.lua_pushboolean(1))
  elseif ast.tag == 'False' then
    return cexpr(C.lua_pushboolean(0))
  elseif ast.tag == 'Number' then
    local cast = cexpr()
    cast:append(C.lua_pushnumber(gennumber(ast[1], cast.pre)))
    return cast
  elseif ast.tag == 'String' then
    local s = ast[1]
    return cexpr(C.lua_pushliteral(s))
  elseif ast.tag == 'Function' then
    return genfunction(ast, where)
  elseif ast.tag == 'Table' then
    return gentable(ast, where)
  elseif ast.tag == 'Op' then
    local opid = ast[1]
    if is_binopid[opid] then
      if is_arithbinop[opid] then
        return genarithbinop(ast, where)
      elseif opid == 'eq' then
        return geneqop(ast, where)
      elseif opid == 'lt' then
        return genltop(ast, where)
      elseif opid == 'le' then
        return genleop(ast, where)
      elseif opid == 'or' or opid == 'and' then
        return genlogbinop(ast, where)
      elseif opid == 'concat' then
        return genconcatop(ast, where)
      else
        assert(false, opid)
      end
    elseif is_unopid[opid] then
      if opid == 'not' then
        return gennotop(ast, where)
      elseif opid == 'len' then
        return genlenop(ast, where)
      elseif opid == 'unm' then
        return genunmop(ast, where)
      else
        assert(false, opid)
      end
    else
      assert(false, opid)
    end
  elseif ast.tag == 'Paren' then
    local ast2 = ast[1]
    return genexpr(ast2, where)
  -- note: metalua allows `Stat here too as an extension
  elseif ast.tag == 'Call' or ast.tag == 'Invoke' then
    return gencall(ast, where, nret)
  elseif ast.tag == 'Id' then
    local name = ast[1]
    local cast = cexpr()
    if _currentscope[name] then -- local
      local idx = localidx(name)
      if tag(idx) == 'Upval' then
        local closuretable_idx, closurelevel, varid = idx[1], idx[2], idx[3]
        cast:append(gen_lc_getupvalue(
          get_closuretableidx_cast(),
          _currentscope['.closurelevel'] - closurelevel, varid))
      elseif where == 'anywhere' then
        cast.idx = idx
        return cast
      else
        cast:append(C.lua_pushvalue(idx))
      end
    else -- global
      --old: 
	  cast:append(C.lua_getglobal(name))
      --cast:append(C.lua_getfield(C.C'LUA_ENVIRONINDEX', name))
    end
    return cast
  elseif ast.tag == 'Index' then
    local cast = cexpr()
    local t_ast, k_ast = ast[1], ast[2]
    local t_idx = cast:append(genexpr(t_ast, 'anywhere')).idx
    cast:append(genexpr(k_ast, 'onstack'))
    cast:append(C.lua_gettable(adjustidx(-2, t_idx)))
    if t_idx == -1 then
      cast:append(C.lua_remove(-2))
    end
    return cast
  else
    assert(false, ast.tag)
  end
end

-- Converts Lua l-value expression AST to C AST.
local function genlvalueassign(l_ast)
  local cast = cexpr()
  if l_ast.tag == 'Id' then
    local name = l_ast[1]
    if _currentscope[name] then
      local idx = localidx(name)
      if tag(idx) == 'Upval' then
        local closuretable_idx, closurelevel, varid = idx[1], idx[2], idx[3]
        cast:append(gen_lc_setupvalue(
          get_closuretableidx_cast(),
          _currentscope['.closurelevel'] - closurelevel, varid))
      else
        cast:append(C.lua_replace(idx))
      end
    else  -- global
      --old:
	  cast:append(C.lua_setglobal(name))
      --cast:append(C.lua_setfield(C.C'LUA_ENVIRONINDEX', name))
    end
  elseif l_ast.tag == 'Index' then
    local t_ast, k_ast = l_ast[1], l_ast[2]
    local t_idx = cast:append(genexpr(t_ast, 'anywhere')).idx
    if t_idx == -1 then cast:append(C.lua_insert(-2)) end
    cast:append(genexpr(k_ast, 'onstack'))
    cast:append(C.lua_insert(-2))
    cast:append(C.lua_settable(adjustidx(-3, t_idx)))
    if t_idx == -1 then
      cast:append(C.lua_pop(1))
    end
  else
    assert(false, l_ast.tag)
  end
  return cast
end


-- Converts Lua `If AST to C AST.
local function genif(ast, i)
  i = i or 1  -- i > 1 is index in AST node of elseif branch to generate
  local cast = cexpr()
  local if_args = {pre=cast.pre}

		  local currentscope_save = _currentscope

		  local base_idx = _idxtop
  local base_id = nextid()
  cast:append(C.Enum(base_id, base_idx))

		  do
    local expr_ast, body_ast = ast[i], ast[i+1]

		    idxtop_restore(base_idx)

		    -- expression
    local a_idx = cast:append(genexpr(expr_ast, 'anywhere')).idx
    if a_idx ~= -1 then
      if_args[#if_args+1] = tobool(a_idx)
    else
      local id = nextid();
      cast:append(C.Let(id, tobool(-1)))
      cast:append(C.lua_pop(1))
      if_args[#if_args+1] = C.Id(id)
    end

		    -- block
    newscope()
    local block_cast = genblock(body_ast)
    table.insert(if_args, block_cast)
    append_array(cast.pre, block_cast.pre)
    restore_scope(currentscope_save)
  end

		  if ast[i+2] then
    idxtop_restore(base_idx)

		    local currentscope_save = newscope()
    local block_cast
    if not ast[i+3] then
      block_cast = genblock(ast[i+2])
      if block_cast[1] then
        prepend_comment(block_cast[1], 'else')
      end
    else
      block_cast = genif(ast, i+2)
    end
    table.insert(if_args, block_cast)
    append_array(cast.pre, block_cast.pre)
    restore_scope(currentscope_save)
  end
  -- note: idx is (in general) indeterminant now
  -- due to the multiple branches.

		  cast:append(C.If(unpack(if_args)))

		  cast:append(C.lua_settop(realidx(C.Id(base_id))))
  idxtop_restore(base_idx)

		  local start =
    i == 1 and ast.lineinfo.first[3] or ast[i-1].lineinfo.last[3]+1
  prepend_comment(cast, trim(src:sub(start, ast[i].lineinfo.last[3])) ..
                          ' then')
  if i == 1 then
    append_comment_below(cast, 'end')
  end
  local comment = false

		  return cast, comment
end


-- Converts Lua `Fornum AST to C AST.
local function genfornum(ast)
  local name_ast, e1_ast, e2_ast, e3_ast, block_ast =
    ast[1], ast[2], ast[3], ast[5] and ast[4] or ast[5], ast[5] or ast[4]
  local name_id = name_ast[1]; assert(name_ast.tag == 'Id')
  local cast = cexpr()

		  local var_id = nextid() .. '_var';
  local limit_id = nextid() .. '_limit';
  local step_id = nextid() .. '_step';
  local var_idx = cast:append(genexpr(e1_ast, 'anywhere')).idx
  local limit_idx = cast:append(genexpr(e2_ast, 'anywhere')).idx
  local step_idx = e3_ast and cast:append(genexpr(e3_ast, 'anywhere')).idx

		  local nstack = (var_idx   == -1 and 1 or 0) +
                 (limit_idx == -1 and 1 or 0) +
                 (step_idx  == -1 and 1 or 0)

		  var_idx, limit_idx, step_idx = adjustidxs(var_idx, limit_idx, step_idx)


		  local expr_cast =
    C.Op('&&', C.lua_isnumber(var_idx), C.lua_isnumber(limit_idx))
  if step_idx then
    expr_cast = C.Op('&&', expr_cast, C.lua_isnumber(step_idx))
  end
  cast:append(
    C.If(C.Not(expr_cast),
         {C.CallL('luaL_error', "'for' limit must be a number")}))
  cast:append(C.LetMutableDouble(var_id, todouble(var_idx)))
  cast:append(C.LetDouble(limit_id, todouble(limit_idx)))
  if e3_ast then
    cast:append(C.LetDouble(step_id, todouble(step_idx)))
  else
    cast:append(C.LetDouble(step_id, 1.0))
  end

			  cast:append(C.lua_pop(nstack))

			  local while_expr_cast =
    C.Op('||',
      C.Op('&&',
        C.Op('>', C.Id(step_id), 0),
        C.Op('<=', C.Id(var_id), C.Id(limit_id))),
      C.Op('&&',
        C.Op('<=', C.Id(step_id), 0),
        C.Op('>=', C.Id(var_id), C.Id(limit_id))))

			  -- local scope to evaluate block
  local currentscope_save = newscope()

			  local block_cast = cexpr()

			  local base_idx = _idxtop
  local base_id = nextid()
  cast:append(C.Enum(base_id, base_idx))

			  -- create local
  if name_ast.upvalue then
    local ct_idx = block_cast:append(gennewclosuretable()).idx
    activate_closure_table(ct_idx)

			    local varid = nextvarid()
    block_cast:append(C.lua_pushnumber(C.Id(var_id)))
    block_cast:append(C.lua_rawseti(-2, varid))
    _currentscope[name_id] =
      {tag='Upval', ct_idx, _currentscope['.closurelevel'], varid}
  else
    block_cast:append(C.lua_pushnumber(C.Id(var_id)))
    idxtop_change(1)
    _currentscope[name_id] = _idxtop
  end
  block_cast[1].comment =
    string.format("internal: local %s at index %d", name_id, _idxtop)

			  block_cast:append(genblock(block_ast))

			  restore_stack_rel(block_cast, base_idx)
  restore_scope(currentscope_save)

			  block_cast:append(C.C(var_id .. ' += ' .. step_id .. ';')) --IMPROVE?

			  cast:append(C.While(while_expr_cast, block_cast))
  append_array(cast.pre, block_cast.pre)

			  -- note: mixed breaks and locals can leave the stack at an
  -- unknown size, so absolute adjust here.
  restore_stack_abs(cast, C.Id(base_id), base_idx)

			  prepend_comment(cast,
    trim(src:sub(ast.lineinfo.first[3], block_ast.lineinfo.first[3]-1)))
  append_comment_below(cast, 'end')
  local comment = false

			  return cast, comment
end

-- Converts Lua `Forin AST to C AST.
local function genforin(ast)
  local names_ast, exprs_ast, block_ast = ast[1], ast[2], ast[3]
  local cast = cexpr()

			  local multi = can_multi(exprs_ast[1])

			  local base_idx = _idxtop
  local base_id = nextid()
  cast:append(C.Enum(base_id, base_idx))

			  -- loop invisible variables: var, limit, step
  local nlast = multi and 3 + 1 - #exprs_ast or 1
  for i,expr_ast in ipairs(exprs_ast) do
    cast:append(genexpr(expr_ast, 'onstack',
      i==#exprs_ast and math.max(0,nlast)))
  end
  if nlast < 0 then
    cast:append(C.lua_pop(-nlast))
  end
  
  -- dirty hack, used in case of next iterator in a for loop
  cast:append(C.C[[
if (lua_gettop(L) == 2) {
  lua_pushnil(L);
}
  ]])
  
  idxtop_change(3)
  append_comment(cast[1], 'internal: local f, s, var = explist')

		  local base2_idx = _idxtop

		  local block_cast = cexpr(); do

		    -- local scope to evaluate block
    local currentscope_save = newscope()
    local extra = 0
    local ct_idx

		    local is_upvalue = false
    for i,name_ast in ipairs(names_ast) do
      if name_ast.upvalue then is_upvalue = true end
    end
    if is_upvalue then
      ct_idx = block_cast:append(gennewclosuretable()).idx
      activate_closure_table(ct_idx)
      extra = 1
    end

		    -- loop variables and control
    block_cast:append(C.lua_pushvalue(-3 - extra))
    append_comment(block_cast[#block_cast],
      'internal: local var_1, ..., var_n = f(s, var)\n' ..
      '          if var_1 == nil then break end\n' ..
      '          var = var_1')
    block_cast:append(C.lua_pushvalue(-3 - extra))
    block_cast:append(C.lua_pushvalue(-3 - extra))
    block_cast:append(C.lua_call(2,#names_ast))
    idxtop_change(#names_ast)
    block_cast:append(C.If(C.lua_isnil(- #names_ast), {C.Break()}))
    block_cast:append(C.lua_pushvalue(- #names_ast))
    block_cast:append(C.lua_replace(- #names_ast - 2 - extra))
    -- loop variables
    local pos1 = #block_cast
    local idx1 = _idxtop - #names_ast
    do -- locals used as upvalues
      local varids = {}
      local nlocals = 0  -- number of non-up-value-enabled locals found
      for i=#names_ast,1,-1 do
        local name_ast = names_ast[i]
        if name_ast.upvalue then
          local name_id = name_ast[1]; assert(name_ast.tag == 'Id')
          varids[i] = nextvarid()
          if nlocals == 0 then
            block_cast:append(C.lua_rawseti(-1 - i, varids[i]))
          else
            block_cast:append(C.lua_pushvalue(-1 - nlocals))
            block_cast:append(C.lua_rawseti(realidx(ct_idx), varids[i]))
            block_cast:append(C.lua_remove(-1 - nlocals))
          end
          idxtop_change(- 1)
          _currentscope[name_id] =
            {tag='Upval', ct_idx, _currentscope['.closurelevel'], varids[i]}
        else
          nlocals = nlocals + 1
        end
      end
      for i,name_ast in ipairs(names_ast) do if varids[i] then
        append_comment(block_cast[pos1+1],
          string.format("internal: upvalue-enabled local %s with id %d",
                        name_ast[1], varids[i]))
      end end
    end
    if pos1 == #block_cast then
      -- hack: ensure AST node exists in which to place comment.
      block_cast:append(C.C'')
    end
    do -- locals
      local count = 0
      for i,name_ast in ipairs(names_ast) do
        if not name_ast.upvalue then
          local name_id = name_ast[1]; assert(name_ast.tag == 'Id')
          count = count + 1
          _currentscope[name_id] = idx1 + count
          append_comment(block_cast[pos1+1],
            string.format("internal: local %s with idx %d",
                          name_id, _currentscope[name_id]))
        end
      end
    end

		    -- block
    block_cast:append(genblock(block_ast))

		    -- end scope
    restore_stack_rel(block_cast, base2_idx)
    restore_scope(currentscope_save)
  end
  cast:append(C.While(1, block_cast))
  append_array(cast.pre, block_cast.pre)

		  restore_stack_abs(cast, C.Id(base_id), base_idx)

		  prepend_comment(cast,
    trim(src:sub(ast.lineinfo.first[3], block_ast.lineinfo.first[3]-1)))
  append_comment_below(cast, 'end')
  local comment = false

		  return cast, comment
end


-- Converts Lua `While AST to C AST.
local function genwhile(ast)
  local expr_ast, block_ast = ast[1], ast[2]
  local cast = cexpr()

		  local base_idx = _idxtop
  local base_id = nextid()
  cast:append(C.Enum(base_id, base_idx))

		  do
    -- expression
    local block_cast = cexpr(); do
      local expr_idx = block_cast:append(genexpr(expr_ast, 'anywhere')).idx
      block_cast:append(
        C.If(C.Not(tobool(expr_idx)), {C.Break()}))
      if expr_idx == -1 then
        block_cast:append(C.lua_pop(1))
      end

		      -- local scope to evaluate block
      local currentscope_save = newscope()

		      -- block
      block_cast:append(genblock(block_ast))

		      restore_stack_rel(block_cast, base_idx)
      restore_scope(currentscope_save)
    end

		    cast:append(C.While(1, block_cast))
    append_array(cast.pre, block_cast.pre)
  end

		  -- note: mixed breaks and locals can leave the stack at an
  -- unknown size, so absolute adjust here.
  restore_stack_abs(cast, C.Id(base_id), base_idx)

		  prepend_comment(cast,
    trim(src:sub(ast.lineinfo.first[3], block_ast.lineinfo.first[3]-1)))
  append_comment_below(cast, 'end')
  local comment = false

		  return cast, comment
end


-- Converts Lua `Repeat AST to C AST.
local function genrepeat(ast)
  local block_ast, expr_ast = ast[1], ast[2]
  local cast = cexpr()

		  local base_idx = _idxtop
  local base_id = nextid()
  cast:append(C.Enum(base_id, base_idx))

		  do
    -- body
    local block_cast = cexpr(); do
      -- local scope to evaluate block and expression
      local currentscope_save = newscope()

		      -- block
      block_cast:append(genblock(block_ast))

		      -- expression
      local expr_idx = block_cast:append(genexpr(expr_ast, 'anywhere')).idx
      idxtop_change(1)
      block_cast:append(
        C.If(tobool(expr_idx), {C.Break()}))

		      restore_stack_rel(block_cast, base_idx)
      restore_scope(currentscope_save)
    end
    cast:append(C.While(1, block_cast))
    append_array(cast.pre, block_cast.pre)
  end

		  -- note: mixed breaks and locals can leave the stack at an
  -- unknown size, so absolute adjust here.
  restore_stack_abs(cast, C.Id(base_id), base_idx)

		  prepend_comment(cast, 'repeat')
  append_comment_below(cast,
    trim(src:sub(block_ast.lineinfo.last[3]+1, ast.lineinfo.last[3])))
  local comment = false

		  return cast, comment
end

)###"
R"###(

-- Converts Lua `Do AST to C AST.
local function gendo(ast)
  local cast = cexpr()

		  local base_idx = _idxtop

		  -- local scope to evaluate block
  local currentscope_save = newscope()
  cast:append(genblock(ast))
  restore_scope(currentscope_save)
  restore_stack_rel(cast, base_idx)

		  prepend_comment(cast, 'do')
  append_comment_below(cast, 'end')
  local comment = false

		  return cast, comment
end

)###"
R"###(

-- Converts Lua `Local AST to C AST.
local function genlocalstat(ast)
  local names_ast, vals_ast = ast[1], ast[2]
  local cast = cexpr()

		  local ct_idx
  local is_upvalue = false
  for i,name_ast in ipairs(names_ast) do
    is_upvalue = is_upvalue or name_ast.upvalue
  end
  if is_upvalue then
    ct_idx = cast:append(gennewclosuretable()).idx
  end

		  deduce_function_names(names_ast, vals_ast)

		  -- create values
  for i,val_ast in ipairs(vals_ast) do
    cast:append(genexpr(val_ast, 'onstack',
      #names_ast > #vals_ast and i == #vals_ast and can_multi(val_ast) and
      (#names_ast - #vals_ast + 1)))
  end

		  -- cleanup if LHS and RHS lengths don't match
  if #names_ast > #vals_ast and not can_multi(vals_ast[#vals_ast]) then
    cast:append(
      C.lua_settop(
        C.Op('+', C.lua_gettop(), #names_ast - #vals_ast)))
  elseif #names_ast < #vals_ast then
    cast:append(C.lua_pop(#vals_ast - #names_ast))
  end

		  if ct_idx then
    -- activate closure scope (after having generated values)
    activate_closure_table(ct_idx)
  end

		  -- store values in closure table and create local symbols
  for i=#names_ast,1,-1 do local name_ast = names_ast[i]
    local name = name_ast[1]

		    if name_ast.upvalue then
      local varid = nextvarid()
      cast:append(C.lua_rawseti(realidx(ct_idx), varid))
      -- create local symbol
      _currentscope[name] =
        {tag='Upval', ct_idx, _currentscope['.closurelevel'], varid}
    end
  end
  -- create local symbols
  for i,name_ast in ipairs(names_ast) do
    local name = name_ast[1]; assert(name_ast.tag == 'Id')
    if not name_ast.upvalue then
      idxtop_change(1)
      _currentscope[name] = _idxtop
    end
  end
  return cast
end

-- Converts Lua `Set AST to C AST.
local function genset(stat_ast)
  -- note: supports x,y=y,x
  local ls_ast, rs_ast = stat_ast[1], stat_ast[2]
  local cast = cexpr()

		  deduce_function_names(ls_ast, rs_ast)

		  -- create values (r_ast)
  for i,r_ast in ipairs(rs_ast) do
    cast:append(genexpr(r_ast, 'onstack',
      #ls_ast > #rs_ast and i == #rs_ast and can_multi(r_ast) and
      (#ls_ast - #rs_ast + 1)))
  end

		  -- cleanup if LHS and RHS lengths don't match
  if #ls_ast > #rs_ast and not can_multi(rs_ast[#rs_ast]) then
    cast:append(
      C.lua_settop(
        C.Op('+', C.lua_gettop(), #ls_ast - #rs_ast)))
  elseif #ls_ast < #rs_ast then
    cast:append(C.lua_pop(#rs_ast - #ls_ast))
  end

		  for i=#ls_ast,1,-1 do
    cast:append(genlvalueassign(ls_ast[i]))
  end

		  return cast
end

-- Converts Lua `Localrec AST to C AST.
local function genlocalrec(ast)
  local names_ast, vals_ast = ast[1], ast[2]
  assert(#names_ast == 1)
  assert(#vals_ast == 1)
  local name_ast = names_ast[1]
  local val_ast = vals_ast[1]
  assert(val_ast.tag == 'Function')

		  local cast = cexpr()
  -- activate scope and symbol (prior to generating value)
  local ct_idx
  local varid
  local name = name_ast[1]
  if name_ast.upvalue then
    ct_idx = cast:append(gennewclosuretable()).idx
    activate_closure_table(ct_idx)
    -- create local symbol
    varid = nextvarid()
    _currentscope[name] =
      {tag='Upval', ct_idx, _currentscope['.closurelevel'], varid}
  else
    _currentscope[name] = _idxtop + 1
  end

		  deduce_function_names(names_ast, vals_ast)

		  -- create value
  cast:append(genexpr(val_ast, 'onstack'))

		  -- store value
  if name_ast.upvalue then
    cast:append(C.lua_rawseti(realidx(ct_idx), varid))
  else
    idxtop_change(1)
  end

		  return cast
end

-- Converts any Lua statement AST to C AST.
local function genstatement(stat_ast)
  local cast
  local comment
  if stat_ast.tag == 'Set' then
    cast = genset(stat_ast)
  elseif stat_ast.tag == 'Return' then
    cast = cexpr()
    local can_multi = #stat_ast >= 1 and can_multi(stat_ast[#stat_ast])
    local id
    if can_multi then
      id = nextid(); cast:append(C.Let(id, C.lua_gettop()))
    end
    for i,e_ast in ipairs(stat_ast) do
      cast:append(genexpr(e_ast, 'onstack',
        i==#stat_ast and can_multi and 'multret' or 1))
    end
    if id then
      cast:append(C.Return(C.Op('-', C.lua_gettop(), C.Id(id))))
    else
      cast:append(C.Return(#stat_ast))
    end
  elseif stat_ast.tag == 'Fornum' then
    cast, comment = genfornum(stat_ast)
  elseif stat_ast.tag == 'Forin' then
    cast, comment = genforin(stat_ast)
  elseif stat_ast.tag == 'While' then
    cast, comment = genwhile(stat_ast)
  elseif stat_ast.tag == 'Repeat' then
    cast, comment = genrepeat(stat_ast)
  elseif stat_ast.tag == 'Do' then
    cast, comment = gendo(stat_ast)
  elseif stat_ast.tag == 'If' then
    cast, comment = genif(stat_ast)
  elseif stat_ast.tag == 'Call' or stat_ast.tag == 'Invoke' then
    cast = genexpr(stat_ast, 'onstack', 0)
  elseif stat_ast.tag == 'Local' then
    cast = genlocalstat(stat_ast)
  elseif stat_ast.tag == 'Localrec' then
    cast = genlocalrec(stat_ast)
  elseif stat_ast.tag == 'Break' then
    cast = cexpr(C.Break())
  else
    assert(false, stat_ast.tag)
  end
  if comment ~= false then
    comment = comment or get_ast_string(src, stat_ast)
    prepend_comment(cast, comment)
  end
  return cast
end

-- Converts Lua block AST to C AST.
-- (local)
function genblock(ast)
  local cast = cexpr()
  for _,stat_ast in ipairs(ast) do
    local stat_cast = genstatement(stat_ast)

		    local comments = stat_ast.lineinfo.first.comments
    if comments then
      for i=#comments,1,-1 do
        local comment = src:sub(comments[i][2], comments[i][3]):gsub('\n$','')
        prepend_comment(stat_cast, comment)
      end
    end

		    cast:append(stat_cast)

		    -- DEBUG
    if true then
      if _funcinfo.is_vararg then
        _funcinfo.is_lc_nextra_used_debug = true
      end
      --cast:append(C.C(string.format([[assert(lua_gettop(L) %s== %d);]],
      --  _funcinfo.is_lc_nextra_used_debug and "- lc_nextra " or "", _idxtop)))
    end


		  end

		  if #ast > 0 then
    local stat_ast = ast[#ast]
    local comments = stat_ast.lineinfo.last.comments
    if comments then
      for i=1,#comments do
        local comment = src:sub(comments[i][2], comments[i][3]):gsub('\n$','')
        append_comment_below(cast[#cast], comment)
      end
    end
  else
    local comments = ast.lineinfo.first.comments
    if comments then
      for i=1,#comments do
        local comment = src:sub(comments[i][2], comments[i][3]):gsub('\n$','')
        append_comment_below(cast, comment)
      end
    end
  end

		  return cast
end

local function gendefinitions(ast)
  local cast = C.Def()
  for _,def_ast in ipairs(ast) do
    cast:append(genstatement(def_ast))
  end
  return cast
end


-- Converts Lua top-level function to C AST,
-- including prelude.
local function genfull(ast)
  -- support LUA_INIT environment variable
  local enable_lua_init = true

		  local cast = cexpr(); cast.tag = 'Def'
  cast:append(genmainchunk(ast))
  return cast
end
M.genfull = genfull


-- First pass through AST <ast>.
-- Each `Id node is marked with field <upvalue=true> if it is used as an
-- upvalue.
-- Each `Function node is marked with field <uses_upvalue=true> if it uses
-- at least one upvalue.
local function first_pass(ast)
  -- Represents current lexical scope.
  -- Maps variable name to variable info (see newvar).
  local scope = {}

		  local functions = {}

		  local function newscope()
    local saved_scope = scope
    scope = setmetatable({}, {__index=scope})
    return saved_scope
  end

		  local function endscope(saved_scope)
    scope = assert(saved_scope)
  end

		  local function newvar(id_ast)
    assert(id_ast.tag == 'Id', id_ast)
    local name = id_ast[1]
    scope[name] = {function_level = #functions, ast = id_ast}
  end

		  local function usevar(name)
    local varinfo = scope[name]
    if varinfo and varinfo.function_level < #functions then
      --DEBUG('upval:', varinfo.ast)
      varinfo.ast.upvalue = true
      for i=varinfo.function_level+1,#functions do
        functions[i].uses_upvalue = true
      end
    end
  end

		  local function process(ast)
    if ast.tag == nil or ast.tag == 'Do' then  -- block
      local saved_scope = newscope()
      for _,stat_ast in ipairs(ast) do
        process(stat_ast)
      end
      endscope(saved_scope)
    elseif ast.tag == 'Set' then
      for i=1,#ast[1] do process(ast[1][i]) end
      for i=1,#ast[2] do process(ast[2][i]) end
    elseif ast.tag == 'While' then
      process(ast[1])
      local saved_scope = newscope()
      process(ast[2])
      endscope(saved_scope)
    elseif ast.tag == 'Repeat' then
      local saved_scope = newscope()
      process(ast[1])
      process(ast[2])
      endscope(saved_scope)
    elseif ast.tag == 'If' then
      for i=1,#ast do
        if i % 2 == 0 or i == #ast then
          local saved_scope = newscope()
          process(ast[i])
          endscope(saved_scope)
        else
          process(ast[i])
        end
      end
    elseif ast.tag == 'Fornum' then
      local saved_scope = newscope()
      newvar(ast[1])
      for i=2,#ast do process(ast[i]) end
      endscope(saved_scope)
    elseif ast.tag == 'Forin' then
      local saved_scope = newscope()
      for i=1,#ast[1] do newvar(ast[1][i]) end
      for i=1,#ast[2] do process(ast[2][i]) end
      process(ast[#ast])
      endscope(saved_scope)
    elseif ast.tag == 'Local' then
      if ast[2] then
        for i=1,#ast[2] do process(ast[2][i]) end
      end
      for i=1,#ast[1] do newvar(ast[1][i]) end
    elseif ast.tag == 'Localrec' then
      for i=1,#ast[1] do newvar(ast[1][i]) end
      if ast[2] then
        for i=1,#ast[2] do process(ast[2][i]) end
      end
    --metalua: elseif ast.tag == 'Goto' or ast.tag == 'Label' then
    elseif ast.tag == 'Return' then
      for i=1,#ast do process(ast[i]) end
    elseif ast.tag == 'Break' then
    elseif ast.tag == 'Nil' or ast.tag == 'Dots' or ast.tag == 'True'
           or ast.tag == 'False' or ast.tag == 'Number' or ast.tag == 'String'
    then
    elseif ast.tag == 'Function' then
      local saved_scope = newscope()
      table.insert(functions, ast)
      for i=1,#ast[1] do
        if ast[1][i].tag ~= 'Dots' then newvar(ast[1][i]) end
      end
      process(ast[2])
      table.remove(functions)
      endscope(saved_scope)
    elseif ast.tag == 'Table' then
      for i=1,#ast do process(ast[i]) end
    elseif ast.tag == 'Pair' then
      for i=1,2 do process(ast[i]) end
    elseif ast.tag == 'Op' then
      for i=2,#ast do process(ast[i]) end
    elseif ast.tag == 'Paren' then
      process(ast[1])
    -- metalua: elseif ast.tag == 'Stat' then
    elseif ast.tag == 'Call' then
      for i=1,#ast do process(ast[i]) end
    elseif ast.tag == 'Invoke' then
      process(ast[1])
      for i=3,#ast do process(ast[i]) end
    elseif ast.tag == 'Index' then
      for i=1,2 do process(ast[i]) end
    elseif ast.tag == 'Id' then
      usevar(ast[1])
    else
      assert(false,  ast.tag)
    end
  end


		  process(ast)
end


first_pass(ast)

local cast = genfull(ast)

return cast


--##------------------------------------------------------------------
--## Note: this is a large function nesting many closures;
--## indentation of its contents is omitted.
--##------------------------------------------------------------------
end
-- end of ast_to_cast


M.ast_to_cast = ast_to_cast
)###";

std::string cast2string_lua = R"###(
--
-- cast2string.lua
-- Converts C AST to C code string.
--
-- (c) 2008 David Manura.  Licensed in the same terms as Lua (MIT license).
-- See included LICENSE file for full licensing details.

_G.N = {}

local _G       = _G
local assert   = _G.assert
local ipairs   = _G.ipairs
local math     = _G.math
local string   = _G.string
local table    = _G.table
local tostring = _G.tostring
local type     = _G.type
local unpack   = _G.unpack

-- Indents lines of code.
local function indentcode(code)
  local indent = '  '
  code = code:gsub('\n', '\n' .. indent)
  code = code:gsub(indent .. '$', '')
  if code ~= "" then code = indent .. code end
  return code
end


-- Makes C comment of string s.
local function ccomment(s)
  s = s:gsub('%*%/', '* /')
  s = s:gsub('%/%*', '/ *')
  s = '/* ' .. s:gsub('\n', '\n' .. ' * ') .. ' */'
  return s
end


-- Tests whether C AST node of given type should not have semicolon
-- appended.
local no_semicolon = {
  ['If'] = true,
  ['While'] = true,
  ['Functiondef'] = true,
  ['C'] = true
}

-- Converts C AST to C code string.
local function cast_to_string(cast)
--  DEBUG(type(cast) == 'table' and cast.tag or cast)
  if type(cast) ~= 'table' then
    if type(cast) =='number' then  -- convenience function
      local s =
        (cast ~= cast) and '(0.0/0.0)' or
        (cast ==  math.huge) and  'HUGE_VAL' or
        (cast == -math.huge) and '-HUGE_VAL' or
        tostring(cast)
      --note: HUGE_VAL/-HUGE_VAL defined in math.h

		      --IMPROVE: avoid 'warning: integer constant is too large for
      -- "long" type', at least in gcc.  Make distinction between
      -- doubles and integers?
      --if not s:find('[Ee%.]') then
      --  s = s .. '.0'
      --end
      return s
    elseif type(cast) == 'string' then  -- convenience function
      return string.format("%q", cast):gsub('\\\n', '\\n')
    else
      assert(false, type(cast))
    end
  elseif cast.tag == 'C' or cast.tag == 'Id' then
    local ccode = cast[1]
    assert(type(ccode) == 'string', tostring(ccode))
    return ccode
  elseif cast.tag == 'Op' then
    local opid, a_cast, b_cast = cast[1], cast[2], cast[3]
    local pa,pz = '(', ')'  -- improve: sometimes can be avoided
    return pa .. cast_to_string(a_cast) ..
           ' ' .. opid .. ' ' .. cast_to_string(b_cast) .. pz
  elseif cast.tag == 'Include' then
    local name = cast[1]
    return '#include ' .. name
  elseif cast.tag == 'Let' then
    local id, val_cast = cast[1], cast[2]
    return "const int " .. id .. " = " .. cast_to_string(val_cast)
  elseif cast.tag == 'LetDouble' then
    local id, val_cast = cast[1], cast[2]
    return "const double " .. id .. " = " .. cast_to_string(val_cast)
  elseif cast.tag == 'LetMutableDouble' then
    local id, val_cast = cast[1], cast[2]
    return "double " .. id .. " = " .. cast_to_string(val_cast)
  elseif cast.tag == 'LetInt' then
    local id, val_cast = cast[1], cast[2]
    return "const int " .. id .. " = " .. cast_to_string(val_cast)
  elseif cast.tag == 'Enum' then
    local id, val_cast = cast[1], cast[2]
    return "enum { " .. id .. " = " .. tostring(val_cast) .. " }"
  elseif cast.tag == 'Not' then
    local a_ast = cast[1]
    local pa,pz = '(', ')'  -- improve: sometimes can be avoided
    return '!' .. pa .. cast_to_string(a_ast) .. pz
  elseif cast.tag == 'Return' then
    local a_ast = cast[1]
    return 'return' .. (a_ast and ' ' .. cast_to_string(a_ast) or '')
  elseif cast.tag == 'Break' then
    return 'break'
  elseif cast.tag == 'Call' then
    local args = {tag='C'}
    for i=2,#cast do
      args[#args+1] = cast_to_string(cast[i])
    end
    return cast[1] .. '(' .. table.concat(args, ',') .. ')'
  elseif cast.tag == 'CallL' then
    local args = {tag='C', 'L'}
    for i=2,#cast do
      args[#args+1] = cast_to_string(cast[i])
    end
    return cast[1] .. '(' .. table.concat(args, ',') .. ')'
  elseif cast.tag == 'Def' then
    local ts = {}
    for i,stat_cast in ipairs(cast) do
      ts[i] = cast_to_string(stat_cast) .. '\n\n'
    end
    local ccode = table.concat(ts)
    return ccode
  elseif cast.tag == nil then -- block
    local ts = {}
    if cast.comment then
      ts[#ts+1] = '\n' .. ccomment(cast.comment) .. '\n'
    end
    for i,stat_cast in ipairs(cast) do
      local comment = ''
      if stat_cast.comment then
        comment = '\n' .. ccomment(stat_cast.comment) .. '\n'
      end
      local postcomment = ''
      if stat_cast.postcomment then
        postcomment = ccomment(stat_cast.postcomment) .. '\n'
      end
      local semi = no_semicolon[stat_cast.tag] and '' or ';'
      ts[#ts+1] = comment .. cast_to_string(stat_cast) .. semi .. '\n' ..
              postcomment
    end
    if cast.postcomment then
      ts[#ts+1] = '\n' .. ccomment(cast.postcomment) .. '\n'
    end

		    local ccode = indentcode(table.concat(ts))
    return ccode
  elseif cast.tag == 'If' then
    local ccode = ''
    for i=2,#cast,2 do
      if i ~= 2 then ccode = ccode .. 'else ' end
      ccode = ccode .. 'if (' .. cast_to_string(cast[i-1]) .. ') {\n' ..
              cast_to_string(cast[i]) .. '}'
    end
    if #cast % 2 == 1 then
      ccode = ccode .. '\nelse {\n' .. cast_to_string(cast[#cast]) .. '}'
    end
    return ccode
  elseif cast.tag == 'While' then
    local expr_cast, block_cast = cast[1], cast[2]
    local ccode = 'while (' .. cast_to_string(expr_cast) .. ') {\n' ..
                  cast_to_string(block_cast) .. '}'
    return ccode
  elseif cast.tag == 'Functiondef' then
    local id, body_cast = cast[1], cast[2]
    local comment = ''
    if cast.comment then
      comment = ccomment(cast.comment) .. '\n'
    end
    local postcomment = ''
    if cast.postcomment then
      postcomment = ccomment(cast.postcomment) .. '\n'
    end
    local ccode =
      comment ..
      'static int ' .. id .. ' (DWORD L) {\n' ..
      cast_to_string(body_cast) .. '}\n' .. postcomment
    return ccode
  elseif cast.tag:find'lua_' == 1 then  -- convenience function
    return cast_to_string{tag='CallL', cast.tag, unpack(cast)}
  else
    assert(false, cast.tag)
  end
end
N.cast_to_string = cast_to_string

)###";

std::string lua2c_lua = R"###(
local _G           = _G
local assert       = _G.assert
local error        = _G.error
local io           = _G.io
local ipairs       = _G.ipairs
local os           = _G.os
local package      = _G.package
local require      = _G.require
local string       = _G.string
local table        = _G.table

_G.mlc = {} -- make gg happy
local mlp = assert(_G.mlp)
local A2C = M
local C2S = N

-- Converts Lua source string to Lua AST (via mlp/gg)
local function string_to_ast(src)
  local  lx  = mlp.lexer:newstream (src)
  local  ast = mlp.chunk (lx)
  return ast
end

src = src:gsub('^#[^\r\n]*', '') -- remove any shebang

local ast = string_to_ast(src)
local cast = A2C.ast_to_cast(src, ast)

code = C2S.cast_to_string(cast)
)###";

std::string code_bridge = R"###(
void main() {
  lcf_main(GetLuaState());
}
)###";

std::string code_headers = R"###(

#define NULL 0
typedef unsigned int DWORD;

#define LUA_MULTRET	(-1)
#define LUA_REGISTRYINDEX       (-10000)
#define LUA_ENVIRONINDEX        (-10001)
#define LUA_GLOBALSINDEX        (-10002)
#define lua_upvalueindex(i)     (LUA_GLOBALSINDEX-(i))

#define R_LUA_TNONE             (-1)
#define R_LUA_TNIL				0
#define R_LUA_TLIGHTUSERDATA	1
#define R_LUA_TNUMBER			2
#define R_LUA_TBOOLEAN			3
#define R_LUA_TSTRING			4
#define R_LUA_TTHREAD			5
#define R_LUA_TFUNCTION			6
#define R_LUA_TTABLE			7
#define R_LUA_TUSERDATA			8
#define R_LUA_TPROTO			9
#define R_LUA_TUPVAL			10

#define lua_getglobal(L,s)			lua_getfield(L, LUA_GLOBALSINDEX, (s))
#define lua_setglobal(L,s)			lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_gettable(L,i)			(lua_getfield(L, i, lua_tostring(L, -1)), lua_remove(L, -2))
#define lua_settable(L,i)			(lua_setfield(L, i, lua_tostring(L, -2)), lua_remove(L, -1))
#define lua_pop(L,n)				lua_settop(L, -(n)-1)
#define lua_newtable(L)				lua_createtable(L, 0, 0)
#define lua_register(L,n,f)			(lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))
#define lua_pushcfunction(L,f)		lua_pushcclosure(L, (int)f, 0)
#define lua_pushnumber(L,n)			lua_pushnum(L, (double)n)
#define lua_strlen(L,i)				lua_objlen(L, (i))
#define lua_tostring(L,i)			lua_tolstring(L, (i), NULL)
#define	lua_pushliteral(L,s)		lua_pushstring(L, s)
#define lua_call(L,a,r)				lua_pcall(L,a,r,0)

#define lua_isfunction(L,n)			(lua_type(L, (n)) == R_LUA_TFUNCTION)
#define lua_istable(L,n)			(lua_type(L, (n)) == R_LUA_TTABLE)
#define lua_islightuserdata(L,n)	(lua_type(L, (n)) == R_LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)				(lua_type(L, (n)) == R_LUA_TNIL)
#define lua_isboolean(L,n)			(lua_type(L, (n)) == R_LUA_TBOOLEAN)
#define lua_isthread(L,n)			(lua_type(L, (n)) == R_LUA_TTHREAD)
#define lua_isnone(L,n)				(lua_type(L, (n)) == R_LUA_TNONE)
#define lua_isnoneornil(L, n)		(lua_type(L, (n)) <= 0)

)###";

