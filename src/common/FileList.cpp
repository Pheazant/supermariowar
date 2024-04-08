#include "FileList.h"

#include "dirlist.h"
#include "GlobalConstants.h"
#include "linfunc.h"

#include <algorithm>
#include <fstream>
#include <list>
#include <cstring>

extern const char * g_szMusicCategoryNames[MAXMUSICCATEGORY];

extern MusicList* musiclist;
extern WorldMusicList* worldmusiclist;

namespace {
const short g_iDefaultMusicCategory[MAXMUSICCATEGORY] {0, 0, 0, 0, 0, 1, 2, 3, 0, 0, 0};
}// namespace


struct MapMusicOverride {
    std::string mapname;
    std::vector<std::string> songs;
};

struct WorldMusicOverride {
    std::string worldname;
    std::string song;
};

std::vector<MapMusicOverride> mapmusicoverrides;
std::vector<WorldMusicOverride> worldmusicoverrides;

void UpdateMusicWithOverrides()
{
    enum class Category : unsigned char {
        None,
        Maps,
        Worlds,
    };

    std::ifstream file(convertPath("music/Overrides.txt"));
    if (!file)
        return;

    Category currentCategory = Category::None;
    std::string line;
    while (std::getline(file, line)) {
        //Ignore comment lines
        if (line.empty() || line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == ' ' || line[0] == '\t')
            continue;

        //Chop off line ending
        line = line.substr(0, line.find('\r'));
        line = line.substr(0, line.find('\n'));

        //If we found a category header
        if (line[0] == '[') {
            std::transform(line.begin(), line.end(), line.begin(), ::tolower);

            if (line == "[maps]")
                currentCategory = Category::Maps;
            else if (line == "[worlds]")
                currentCategory = Category::Worlds;

            continue;
        }

        //If we're not in a category, ignore this line
        if (currentCategory == Category::None)
            continue;

        std::list<std::string> tokens = tokenize(line, ',');
        if (tokens.empty())
            continue;

        if (currentCategory == Category::Maps) {
            MapMusicOverride override;

            override.mapname = std::move(tokens.front());
            tokens.pop_front();

            for (const std::string& token : tokens) {
                std::string path = convertPath(token);
                if (File_Exists(path))
                    override.songs.emplace_back(std::move(path));
            }
            //Don't add overrides that have no songs
            if (!override.songs.empty())
                mapmusicoverrides.emplace_back(std::move(override));
        }
        else if (currentCategory == Category::Worlds) {
            WorldMusicOverride override;

            override.worldname = std::move(tokens.front());
            tokens.pop_front();

            std::string path = convertPath(tokens.front());
            if (File_Exists(path)) {
                override.song = std::move(path);
                worldmusicoverrides.emplace_back(std::move(override));
            }
        }
    }

    musiclist->UpdateEntriesWithOverrides();
    worldmusiclist->UpdateEntriesWithOverrides();
}

///////////// SimpleFileList ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SimpleFileList::SimpleFileList()
    : currentIndex(-1)
{}

SimpleFileList::SimpleFileList(const std::string &path, const std::string &extension, bool fAlphabetize)
{
    DirectoryListing d(path, extension);
    std::string curname;
    while (d(curname)) {
        filelist.insert(filelist.end(), d.fullName(curname));
    }

    currentIndex = 0;

    if (filelist.empty()) {
#pragma warning "Print this line to log"
        //printf("ERROR: Empty directory!\n");
        currentIndex = -1;
    }

    //Alphabetize the list, ignoring author
    if (fAlphabetize) {
        short iSize = filelist.size();
        std::string * names = new std::string[iSize];

        //Get only the names of the files, no author information
        for (short i = 0; i < iSize; i++) {
            names[i] = stripPathAndExtension(filelist[i]);
            std::transform(names[i].begin(), names[i].end(), names[i].begin(), tolower);
        }

        //Now bubblesort them
        bool fDone = false;
        while (!fDone) {
            fDone = true;
            for (short i = 0; i < iSize - 1; i++) {
                if (names[i].compare(names[i + 1]) > 0) {
                    fDone = false;
                    std::string tempName = names[i];
                    names[i] = names[i + 1];
                    names[i + 1] = tempName;

                    std::string tempFullName = filelist[i];
                    filelist[i] = filelist[i + 1];
                    filelist[i + 1] = tempFullName;
                }
            }
        }

        delete [] names;
    }
}

SimpleFileList::~SimpleFileList()
{
    filelist.clear();
}

void SimpleFileList::SetCurrent(unsigned int index)
{
    if (filelist.empty())
        return;

    if (index < filelist.size())
        currentIndex = index;
    else
        currentIndex = 0;
};

const char * SimpleFileList::current_name()
{
    if (currentIndex > -1)
        return filelist[currentIndex].c_str();

    return NULL;
};

void SimpleFileList::next()
{
    if (filelist.empty())
        return;

    if (currentIndex + 1 == int(filelist.size()))
        currentIndex = 0;
    else
        currentIndex++;
}

void SimpleFileList::prev()
{
    if (filelist.empty())
        return;

    if (currentIndex == 0)
        currentIndex = filelist.size() - 1;
    else
        currentIndex--;
}

void SimpleFileList::random()
{
    if (!filelist.empty())
        currentIndex = RANDOM_INT(filelist.size());
};

const char * SimpleFileList::GetIndex(unsigned int index)
{
    if (index < filelist.size())
        return filelist[index].c_str();

    return NULL;
}

void SimpleFileList::SetCurrentName(const std::string &name)
{
    if (filelist.empty())
        return;

    for (unsigned short i = 0; i < filelist.size(); i++) {
        if (!strcmp(filelist[i].c_str(), name.c_str())) {
            currentIndex = i;
            break;
        }
    }
}

void SimpleFileList::add(const char * name)
{
    filelist.push_back(name);
}

bool SimpleFileList::find(const char * name)
{
    bool fFound = false;

    int oldCurrent = currentIndex;
    do {
        next(); //sets us to the beginning if we hit the end -> loop through the maps

        if (strstr(filelist[currentIndex].c_str(), name))   //compare names after
            fFound = true;
    } while (currentIndex != oldCurrent && !fFound);

    return fFound;
}

///////////// SkinList ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SkinListNode::SkinListNode(std::string skinName, std::string skinPath)
{
    sSkinName = skinName;
    sSkinPath = skinPath;
}


SkinList::SkinList()
{
    DirectoryListing d(convertPath("gfx/skins/"));
    std::string curname;
    while (d(curname)) {
        if (curname.length() < 5)
            continue;

        std::string file_ext = curname.substr(curname.length() - 4);
        inPlaceLowerCase(file_ext);
        if (file_ext != ".bmp" && file_ext != ".png") //Allow bmp and png skins
            continue;

        std::string sShortSkinName = stripCreatorAndDotMap(curname);
        SkinListNode * node = new SkinListNode(sShortSkinName, d.fullName(curname));

        if (skins.empty()) {
            skins.push_back(node);
        } else {
            std::vector<SkinListNode*>::iterator itr = skins.begin(), lim = skins.end();

            while (itr != lim) {
                if (sShortSkinName.compare((*itr)->sSkinName) < 0)
                    break;

                itr++;
            }

            skins.insert(itr, node);
        }
    }
}

const char * SkinList::GetIndex(unsigned int index)
{
    if (index < skins.size())
        return skins[index]->sSkinPath.c_str();

    return NULL;
}

const char * SkinList::GetSkinName(unsigned int index)
{
    if (index < skins.size())
        return skins[index]->sSkinName.c_str();

    return NULL;
}

///////////// SimpleDirectoryList ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SimpleDirectoryList::SimpleDirectoryList(const std::string &path)
{
    DirectoryListing d(path);
    std::string curname;
    while (d.NextDirectory(curname)) {
        filelist.insert(filelist.end(), d.fullName(curname));
    }
    if (filelist.empty()) {
        printf("ERROR: Empty directory.  %s\n", path.c_str());
        //exit(0);
    }

    currentIndex = 0;
}


///////////// MusicList ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MusicList::MusicList()
{
    DirectoryListing d(convertPath("music/game/"));
    std::string currentdir;
    while (d.NextDirectory(currentdir)) {
        MusicEntry *m = new MusicEntry(d.fullName(currentdir));
        if (!m->fError)
            entries.push_back(m);
        else
            delete m;
    }

    if (entries.empty()) {
        throw "Empty Music directory!";
    }

    currentIndex = 0;
}

MusicList::~MusicList()
{
    for (unsigned int i = 0; i < entries.size(); i++) {
        delete entries[i];
    }

    entries.clear();
}

std::string MusicList::GetMusic(int musicID)
{
    return entries[currentIndex]->GetMusic(musicID);
}

void MusicList::SetRandomMusic(int iMusicCategory, const char * szMapName, const char * szBackground)
{
    CurrentMusic = entries[currentIndex]->GetRandomMusic(iMusicCategory, szMapName, szBackground);
}

void MusicList::SetNextMusic(int iMusicCategory, const char * szMapName, const char * szBackground)
{
    CurrentMusic = entries[currentIndex]->GetNextMusic(iMusicCategory, szMapName, szBackground);
}

std::string MusicList::GetCurrentMusic()
{
    return CurrentMusic;
}


void MusicList::next()
{
    if (currentIndex+1 == int(entries.size()))
        currentIndex = 0;
    else
        currentIndex++;
}

void MusicList::prev()
{
    if (currentIndex == 0)
        currentIndex = entries.size()-1;
    else
        currentIndex--;
}

void MusicList::UpdateEntriesWithOverrides()
{
    for (unsigned short i = 0; i < entries.size(); i++) {
        entries[i]->UpdateWithOverrides();
    }
}


///////////// MusicEntry ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MusicEntry::MusicEntry(const std::string & musicdirectory)
{
    fError = false;
    fUsesMapOverrides = false;
    fUsesBackgroundOverrides = false;

    iCurrentMusic = 0;

    int i, k;

    for (k = 0; k < MAXMUSICCATEGORY; k++)
        numsongsforcategory[k] = 0;

    size_t separator_pos = musicdirectory.rfind(getDirectorySeperator());
    if (separator_pos != std::string::npos)
        name = musicdirectory.substr(separator_pos + 1);
    else
        name = musicdirectory;

    name = name.substr(0, name.rfind("."));

    std::string musicfile = musicdirectory + getDirectorySeperator() + std::string("Music.txt");
    FILE * in = fopen(musicfile.c_str(), "r");
    if (!in) {
        printf("Error: Could not open: %s\n", musicfile.c_str());
        fError = true;
        return;
    }

    int iNumFile = 0;

    //Run through the global overrides and add them to the map overrides of this music entry
    //if there are any overrides then for each override
    /*
    {
        if (mapoverride.find(pszName) == mapoverride.end())
            mapoverride[pszName] = new MusicOverride();

        songFileNames.push_back(sPath);

        fUsesMapOverrides = true;
        mapoverride[pszName]->songs.push_back(iNumFile);
        iNumFile++;
    }
    */

    int iAddToCategory = -1;
    char szBuffer[256];
    while (fgets(szBuffer, 256, in)) {
        //Ignore comment lines
        if (szBuffer[0] == '#' || szBuffer[0] == ' ' || szBuffer[0] == '\t' || szBuffer[0] == '\n' || szBuffer[0] == '\r')
            continue;

        //Chop off line ending
        int stringLength = strlen(szBuffer);
        for (k = 0; k < stringLength; k++) {
            if (szBuffer[k] == '\r' || szBuffer[k] == '\n') {
                szBuffer[k] = '\0';
                break;
            }
        }

        //If we found a category header
        if (szBuffer[0] == '[') {
            if (cstr_ci_equals(szBuffer, "[land]"))
                iAddToCategory = 0;
            else if (cstr_ci_equals(szBuffer, "[underground]"))
                iAddToCategory = 1;
            else if (cstr_ci_equals(szBuffer, "[underwater]"))
                iAddToCategory = 2;
            else if (cstr_ci_equals(szBuffer, "[castle]"))
                iAddToCategory = 3;
            else if (cstr_ci_equals(szBuffer, "[platforms]"))
                iAddToCategory = 4;
            else if (cstr_ci_equals(szBuffer, "[ghost]"))
                iAddToCategory = 5;
            else if (cstr_ci_equals(szBuffer, "[bonus]"))
                iAddToCategory = 6;
            else if (cstr_ci_equals(szBuffer, "[battle]"))
                iAddToCategory = 7;
            else if (cstr_ci_equals(szBuffer, "[desert]"))
                iAddToCategory = 8;
            else if (cstr_ci_equals(szBuffer, "[clouds]"))
                iAddToCategory = 9;
            else if (cstr_ci_equals(szBuffer, "[snow]"))
                iAddToCategory = 10;
            else if (cstr_ci_equals(szBuffer, "[maps]"))
                iAddToCategory = MAXMUSICCATEGORY;
            else if (cstr_ci_equals(szBuffer, "[backgrounds]"))
                iAddToCategory = MAXMUSICCATEGORY + 1;

            continue;
        }

        //Cap the number of songs at MAXCATEGORYTRACKS for a category
        if (iAddToCategory > -1 && iAddToCategory < MAXMUSICCATEGORY && numsongsforcategory[iAddToCategory] >= MAXCATEGORYTRACKS)
            continue;

        if (iNumFile < 4 || iAddToCategory > -1) {
            if (iAddToCategory == MAXMUSICCATEGORY || iAddToCategory == MAXMUSICCATEGORY + 1) {
                char * pszName = strtok(szBuffer, ",\n");

                if (!pszName)
                    continue;

                if (iAddToCategory == MAXMUSICCATEGORY) {
                    if (mapoverride.find(pszName) == mapoverride.end())
                        mapoverride[pszName] = new MusicOverride();
                } else {
                    if (backgroundoverride.find(pszName) == backgroundoverride.end())
                        backgroundoverride[pszName] = new MusicOverride();
                }

                char * pszMusic = strtok(NULL, ",\n");
                while (pszMusic) {
                    std::string sPath = musicdirectory + getDirectorySeperator() + pszMusic;

                    if (File_Exists(sPath.c_str())) {
                        songFileNames.push_back(sPath);

                        if (iAddToCategory == MAXMUSICCATEGORY) {
                            fUsesMapOverrides = true;
                            mapoverride[pszName]->songs.push_back(iNumFile);
                        } else {
                            fUsesBackgroundOverrides = true;
                            backgroundoverride[pszName]->songs.push_back(iNumFile);
                        }

                        iNumFile++;
                    }
                    pszMusic = strtok(NULL, ",\n");
                }
            } else {
                std::string sPath = musicdirectory + getDirectorySeperator() + szBuffer;

                if (File_Exists(sPath.c_str())) {
                    songFileNames.push_back(sPath);

                    //Don't add to category lists if this is one of the four special music tracks
                    if (iNumFile >= 4) {
                        songsforcategory[iAddToCategory][numsongsforcategory[iAddToCategory]] = iNumFile;
                        numsongsforcategory[iAddToCategory]++;
                    }
                    iNumFile++;
                }
            }
        }
    }

    fclose(in);

    //Now read labeled subdirectories like "Land", "Underground", "Castle", etc for more songs

    for (short iMusicCategory = 0; iMusicCategory < MAXMUSICCATEGORY; iMusicCategory++) {
        std::string musicPath = musicdirectory + getDirectorySeperator() + std::string(g_szMusicCategoryNames[iMusicCategory]);
        if (File_Exists(musicPath)) {
            SimpleFileList musList(musicPath + getDirectorySeperator(), ".ogg");

            short iCount = musList.GetCount();

            //printf("Found %d files in %s\n", iCount, musicPath.c_str());

            for (short iFile = 0; iFile < iCount; iFile++) {
                if (numsongsforcategory[iMusicCategory] < MAXCATEGORYTRACKS) {
                    songFileNames.push_back(musList.current_name());
                    songsforcategory[iMusicCategory][numsongsforcategory[iMusicCategory]] = iNumFile;
                    numsongsforcategory[iMusicCategory]++;
                    iNumFile++;
                }

                musList.next();
            }
        }
    }

    if (iNumFile == 0) {
        printf("Error: No songs found in: %s\n", musicdirectory.c_str());
        fError = true;
        return;
    }

    //Verify we have at least one track for each category
    for (i = 0; i < MAXMUSICCATEGORY; i++) {
        if (numsongsforcategory[i] == 0) {
            if (i < 4) {
                printf("Error: Missing track definition for music category: %s\n", g_szMusicCategoryNames[i]);
                fError = true;
                return;
            } else { //Use default category
                numsongsforcategory[i] = numsongsforcategory[g_iDefaultMusicCategory[i]];
                for (k = 0; k < numsongsforcategory[i]; k++) {
                    songsforcategory[i][k] = songsforcategory[g_iDefaultMusicCategory[i]][k];
                }
            }
        }
    }

    /*
    std::vector<std::string>::iterator itr = songFileNames.begin();

    printf("------ Songlist ------\n");
    short iCounter = 0;
    while (itr != songFileNames.end())
    {
        printf("%d: %s\n", iCounter++, itr->c_str());
        itr++;
    }

    printf("\n\n");

    for (i = 0; i < MAXMUSICCATEGORY; i++)
    {
        printf("---- %d ----\n", i);
        printf("Count: %d\n", numsongsforcategory[i]);
        for (k = 0; k < numsongsforcategory[i]; k++)
            printf("%d\n", songsforcategory[i][k]);
    }
    */
}

std::string MusicEntry::GetMusic(unsigned int musicID)
{
    if (musicID >= songFileNames.size())
        return songFileNames[songFileNames.size()-1];

    return songFileNames[musicID];
}

std::string MusicEntry::GetRandomMusic(int iMusicCategory, const char * szMapName, const char * szBackground)
{
    //First check if there is specific map music
    if (fUsesMapOverrides && mapoverride.find(szMapName) != mapoverride.end()) {
        if (mapoverride[szMapName]->songs.size() > 0) {
            iCurrentMusic = RANDOM_INT( mapoverride[szMapName]->songs.size());
            return songFileNames[mapoverride[szMapName]->songs[iCurrentMusic]];
        }
    }
    //Then check if there is specific background music
    if (fUsesBackgroundOverrides && backgroundoverride.find(szBackground) != backgroundoverride.end()) {
        if (backgroundoverride[szBackground]->songs.size() > 0) {
            iCurrentMusic = RANDOM_INT(backgroundoverride[szBackground]->songs.size());
            return songFileNames[backgroundoverride[szBackground]->songs[iCurrentMusic]];
        }
    }

    //Then default to the music category
    if (iMusicCategory >= 0 && iMusicCategory < MAXMUSICCATEGORY && numsongsforcategory[iMusicCategory] > 0) {
        iCurrentMusic = RANDOM_INT( numsongsforcategory[iMusicCategory]);
        return songFileNames[songsforcategory[iMusicCategory][iCurrentMusic]];
    }

    return songFileNames[4];
}

std::string MusicEntry::GetNextMusic(int iMusicCategory, const char * szMapName, const char * szBackground)
{
    //First check if there is specific map music
    if (mapoverride.find(szMapName) != mapoverride.end()) {
        if (mapoverride[szMapName]->songs.size() > 0) {
            if (++iCurrentMusic >= mapoverride[szMapName]->songs.size())
                iCurrentMusic = 0;

            return songFileNames[mapoverride[szMapName]->songs[iCurrentMusic]];
        }
    }
    //Then check if there is specific background music
    if (backgroundoverride.find(szBackground) != backgroundoverride.end()) {
        if (backgroundoverride[szBackground]->songs.size() > 0) {
            if (++iCurrentMusic >= backgroundoverride[szBackground]->songs.size())
                iCurrentMusic = 0;

            return songFileNames[backgroundoverride[szBackground]->songs[iCurrentMusic]];
        }
    }

    //Then default to the music category
    if (iMusicCategory >= 0 && iMusicCategory < MAXMUSICCATEGORY && numsongsforcategory[iMusicCategory] > 0) {
        if (++iCurrentMusic >= numsongsforcategory[iMusicCategory])
            iCurrentMusic = 0;

        return songFileNames[songsforcategory[iMusicCategory][iCurrentMusic]];
    }

    return songFileNames[4];
}

void MusicEntry::UpdateWithOverrides()
{
    short iNumFile = songFileNames.size();

    if (!mapmusicoverrides.empty())
        fUsesMapOverrides = true;

    for (const MapMusicOverride& override : mapmusicoverrides) {
        if (mapoverride.find(override.mapname) == mapoverride.end())
            mapoverride[override.mapname] = new MusicOverride();

        for (unsigned short j = 0; j < override.songs.size(); j++) {
            songFileNames.push_back(override.songs[j]);
            mapoverride[override.mapname]->songs.push_back(iNumFile);
            iNumFile++;
        }
    }
}

///////////// MusicList ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WorldMusicList::WorldMusicList()
{
    DirectoryListing d(convertPath("music/world/"));
    std::string currentdir;
    while (d.NextDirectory(currentdir)) {
        WorldMusicEntry *m = new WorldMusicEntry(d.fullName(currentdir));
        if (!m->fError)
            entries.push_back(m);
        else
            delete m;
    }

    if (entries.empty()) {
        throw "Empty Music directory!";
    }

    currentIndex = 0;
}

WorldMusicList::~WorldMusicList()
{
    for (unsigned int i = 0; i < entries.size(); i++) {
        delete entries[i];
    }

    entries.clear();
}

std::string WorldMusicList::GetMusic(int musicID, const char * szWorldName)
{
    return entries[currentIndex]->GetMusic(musicID, szWorldName);
}

std::string WorldMusicList::GetCurrentMusic()
{
    return CurrentMusic;
}


void WorldMusicList::next()
{
    if (currentIndex+1 == int(entries.size()))
        currentIndex = 0;
    else
        currentIndex++;
}

void WorldMusicList::prev()
{
    if (currentIndex == 0)
        currentIndex = entries.size()-1;
    else
        currentIndex--;
}

void WorldMusicList::UpdateEntriesWithOverrides()
{
    for (unsigned short i = 0; i < entries.size(); i++) {
        entries[i]->UpdateWithOverrides();
    }
}


///////////// WorldMusicEntry ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WorldMusicEntry::WorldMusicEntry(const std::string & musicdirectory)
{
    fError = false;
    fUsesWorldOverrides = false;

    size_t separator_pos = musicdirectory.rfind(getDirectorySeperator());
    if (separator_pos != std::string::npos)
        name = musicdirectory.substr(separator_pos + 1);
    else
        name = musicdirectory;

    name = name.substr(0, name.rfind("."));

    std::string musicfile = musicdirectory + getDirectorySeperator() + std::string("Music.txt");
    FILE * in = fopen(musicfile.c_str(), "r");
    if (!in) {
        printf("Error: Could not open: %s\n", musicfile.c_str());
        fError = true;
        return;
    }

    int iAddToCategory = -1;
    char szBuffer[256];
    while (fgets(szBuffer, 256, in)) {
        //Ignore comment lines
        if (szBuffer[0] == '#' || szBuffer[0] == ' ' || szBuffer[0] == '\t' || szBuffer[0] == '\n' || szBuffer[0] == '\r')
            continue;

        //Chop off line ending
        int stringLength = strlen(szBuffer);
        for (int k = 0; k < stringLength; k++) {
            if (szBuffer[k] == '\r' || szBuffer[k] == '\n') {
                szBuffer[k] = '\0';
                break;
            }
        }

        //If we found a category header
        if (szBuffer[0] == '[') {
            if (cstr_ci_equals(szBuffer, "[grass]"))
                iAddToCategory = 0;
            else if (cstr_ci_equals(szBuffer, "[desert]"))
                iAddToCategory = 1;
            else if (cstr_ci_equals(szBuffer, "[water]"))
                iAddToCategory = 2;
            else if (cstr_ci_equals(szBuffer, "[giant]"))
                iAddToCategory = 3;
            else if (cstr_ci_equals(szBuffer, "[sky]"))
                iAddToCategory = 4;
            else if (cstr_ci_equals(szBuffer, "[ice]"))
                iAddToCategory = 5;
            else if (cstr_ci_equals(szBuffer, "[pipe]"))
                iAddToCategory = 6;
            else if (cstr_ci_equals(szBuffer, "[dark]"))
                iAddToCategory = 7;
            else if (cstr_ci_equals(szBuffer, "[space]"))
                iAddToCategory = 8;
            else if (cstr_ci_equals(szBuffer, "[bonus]"))
                iAddToCategory = WORLDMUSICBONUS;
            else if (cstr_ci_equals(szBuffer, "[sleep]"))
                iAddToCategory = WORLDMUSICSLEEP;
            else if (cstr_ci_equals(szBuffer, "[worlds]"))
                iAddToCategory = WORLDMUSICWORLDS;

            continue;
        }

        if (iAddToCategory > -1 && iAddToCategory <= WORLDMUSICSLEEP) {
            std::string sPath = musicdirectory + getDirectorySeperator() + szBuffer;

            if (File_Exists(sPath.c_str()))
                songFileNames[iAddToCategory] = sPath;
        } else if (iAddToCategory == WORLDMUSICWORLDS) {
            char * pszName = strtok(szBuffer, ",\n");

            if (!pszName)
                continue;

            char * pszMusic = strtok(NULL, ",\n");

            if (!pszMusic)
                continue;

            std::string sPath = musicdirectory + getDirectorySeperator() + pszMusic;

            if (!File_Exists(sPath.c_str()))
                continue;

            fUsesWorldOverrides = true;
            worldoverride[pszName] = sPath;

        }
    }

    fclose(in);
}

std::string WorldMusicEntry::GetMusic(unsigned int musicID, const char * szWorldName)
{
    //First check if there is specific map music
    if (fUsesWorldOverrides && worldoverride.find(szWorldName) != worldoverride.end()) {
        return worldoverride[szWorldName];
    }

    if (musicID > WORLDMUSICSLEEP)
        return songFileNames[0];

    return songFileNames[musicID];
}

void WorldMusicEntry::UpdateWithOverrides()
{
    if (!worldmusicoverrides.empty())
        fUsesWorldOverrides = true;

    for (const WorldMusicOverride& override : worldmusicoverrides) {
        worldoverride[override.worldname] = override.song;
    }
}
