program wfof_gen;

{$APPTYPE CONSOLE}

{
 * Created by Martin Winkelhofer 02,03/2016
 * W-Dimension / wdim / maarty.w@gmail.com
 *    _____ __          ____         ______         __
 *   / __(_) /__ ___   / __ \___    / __/ /__ ____ / /
 *  / _// / / -_|_-<  / /_/ / _ \  / _// / _ `(_-</ _ \
 * /_/ /_/_/\__/___/  \____/_//_/ /_/ /_/\_,_/___/_//_/
 *
 * This file is part of WFOF - W-Dimension's Files On Flash (for ESP8266).
 *
 * WFOF is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WFOF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WFOF. If not, see <http://www.gnu.org/licenses/>.
}

uses
  SysUtils, Classes;

const
  APP_NAME = 'wfof_gen v1.2';

type
  Fl = record
    Offs: integer;
    Size: integer;
    Name: string;
  end;

var
  CalledDir,InDir,OutFile: string;
  SR: TSearchRec;
  TC1: integer = 0;
  TC2: integer;
  SL: TStringList;
  Buf: array of byte;
  OutBuf: array of byte;
  FilesCnt: integer = 0;
  Files: array of Fl;
  FilesStr: string;
  FilesDataStr: string;
  FilesDataStrLen: integer;
  TmpS: string;
  FilesDataStrOffs: integer;
  Params: string;
  DoUppercase: boolean = false;

procedure AddFileToBuf(FN: string; FullFN: string; Sz: integer);
var
  FH,LastOBLen: integer;
begin
  try
    LastOBLen:=Length(OutBuf);
    SetLength(Files,Length(Files)+1);
    Files[FilesCnt].Offs:=LastOBLen;
    Files[FilesCnt].Size:=Sz;
    Files[FilesCnt].Name:=FN;
    Inc(FilesCnt);
    //
    SetLength(Buf,Sz);
    FH:=FileOpen(FullFN,soFromBeginning);
    FileRead(FH,Buf[0],Sz);
    FileClose(FH);
    //
    SetLength(OutBuf,LastOBLen+Sz);
    Move(Buf[0],OutBuf[LastOBLen],Sz);
  except
    on E:Exception do begin
      Writeln(Output,'Error: '+E.Message);
    end;
  end;
end;

begin
  Writeln(Output,APP_NAME+', created by Martin Winkelhofer 02,03/2016');
  Writeln(Output,'W-Dimension / wdim / maarty.w@gmail.com');
  SetLength(OutBuf,0);
  SetLength(Files,0);
  if(ParamCount<2) then begin
    Writeln(Output,'----------------');
    Writeln(Output,'Usage:');
    Writeln(Output,'wfof_gen.exe <inputdir> <outputfile> (<params>)');
    Writeln(Output,'params: -u uppercase');
    Exit;
  end;
  //
  CalledDir:=ExtractFilePath(ParamStr(0));
  InDir:=ParamStr(1);
  OutFile:=ParamStr(2);
  if((CalledDir<>'')and(CalledDir[Length(CalledDir)]<>'\')) then CalledDir:=CalledDir+'\'; //add last '\' if CalledDir is not empty and it's not ending with '\'
  if(InDir[Length(InDir)]='\') then InDir:=Copy(InDir,1,Length(InDir)); //remove last '\' if any
  //
  SL:=TStringList.Create();
  try
    SL.LoadFromFile(CalledDir+'template\wfof_data.tpl');
  except
    on E:Exception do begin
      Writeln(Output,'Error: '+E.Message);
      Exit;
    end;
  end;
  //
  if(ParamCount>2) then begin
    Params:=ParamStr(3);
    if((Length(Params)>1)and(Params[1]='-')) then begin
      Params:=Copy(Params,2,Length(Params)); //get rid of leading '-'
      DoUppercase:=(Pos('u',Params)<>0);
    end;
  end;
  //
  Writeln(Output,'----------------');
  if(FindFirst(InDir+'\*',faAnyFile,SR)=0) then begin
    repeat
      if((SR.Attr and faDirectory)<>faDirectory) then begin
        Writeln(Output,Format('%s (%d bytes)',[SR.Name,SR.Size]));
        AddFileToBuf(SR.Name,InDir+'\'+SR.Name,SR.Size);
      end;
      //
      Inc(TC1);
      if(TC1>255) then break; //max 255 files
      //
    until(FindNext(SR)<>0);
    FindClose(SR);
  end;
  Writeln(Output,'----------------');
  //
  FilesStr:='';
  for TC2:=0 to FilesCnt-1 do begin //go through all Files[]
    if(DoUppercase) then TmpS:=UpperCase(Files[TC2].Name) else TmpS:=Files[TC2].Name;
    FilesStr:=FilesStr+Format('%s{.Offs=%d, .Size=%d, .Name="%s"},%s',[#9+#9,Files[TC2].Offs,Files[TC2].Size,TmpS,#13+#10]);
  end;
  if(FilesStr<>'') then FilesStr:=Copy(FilesStr,1,Length(FilesStr)-3); //remove last ','+#13+#10
  //
  FilesDataStr:='';
  FilesDataStrLen:=Length(OutBuf)*6; //each byte is exported like this: '0x00, '
  FilesDataStrLen:=FilesDataStrLen+((Length(OutBuf) div 20)*2); //every 20 bytes a new line is added (#13+#10)
  FilesDataStrLen:=FilesDataStrLen-2; //last ', ' is truncated
  if(FilesDataStrLen<0) then FilesDataStrLen:=0;
  SetLength(FilesDataStr,FilesDataStrLen);
  FilesDataStrOffs:=1;
  for TC2:=0 to Length(OutBuf)-1 do begin //go through all bytes in OutBuf
    TmpS:=Format('0x%0.2X, ',[OutBuf[TC2]]);
    if(TC2=Length(OutBuf)-1) then TmpS:=Copy(TmpS,1,Length(TmpS)-2); //last ', ' is truncated
    if((TC2>0)and((TC2+1) mod 20 = 0)) then TmpS:=TmpS+#13+#10;
    Move(TmpS[1],FilesDataStr[FilesDataStrOffs],Length(TmpS));
    FilesDataStrOffs:=FilesDataStrOffs+Length(TmpS);
  end;
  //
  SL.Text:=StringReplace(SL.Text,'[[APPNAME]]',APP_NAME,[rfReplaceAll]);
  SL.Text:=StringReplace(SL.Text,'[[DATETIME]]',DateTimeToStr(Now()),[rfReplaceAll]);
  SL.Text:=StringReplace(SL.Text,'[[FILESCNT]]',IntToStr(FilesCnt),[rfReplaceAll]);
  SL.Text:=StringReplace(SL.Text,'[[FILES]]',FilesStr,[rfReplaceAll]);
  SL.Text:=StringReplace(SL.Text,'[[FILESDATA]]',Trim(FilesDataStr),[rfReplaceAll]);
  //
  Writeln(Output,'Writing '+OutFile+' ...');
  try
    SL.SaveToFile(OutFile);
  except
    on E:Exception do begin
      Writeln(Output,'Error: '+E.Message);
      Exit;
    end;
  end;
  //
  SL.Free();
  Writeln(Output,'DONE');
end.
