#include "MapReader.h"

#include "map.h"
#include "MapReaderConstants.h"
#include "movingplatform.h"
#include "FileIO.h"
#include "TilesetManager.h"

#include <iostream>

extern CTilesetManager* g_tilesetmanager;


namespace {
struct TilesetTranslation {
    short iID;
    char szName[128];
};
}  // namespace


MapReader1800::MapReader1800()
    : MapReader1702()
    , iMaxTilesetID(-1)
    , translationid(NULL)
    , tilesetwidths(NULL)
    , tilesetheights(NULL)
{
    patch_version = 0;
}

MapReader1801::MapReader1801()
    : MapReader1800()
{
    patch_version = 1;
}

MapReader1802::MapReader1802()
    : MapReader1801()
{
    patch_version = 2;
}

void MapReader1800::read_autofilters(CMap& map, BinaryFile& mapfile)
{
    // TODO: handle read fail
    int32_t iAutoFilterValues[NUM_AUTO_FILTERS + 1];
    mapfile.read_i32_array(iAutoFilterValues, NUM_AUTO_FILTERS + 1);

    for (short iFilter = 0; iFilter < NUM_AUTO_FILTERS; iFilter++)
        map.fAutoFilter[iFilter] = iAutoFilterValues[iFilter] > 0;
}

void MapReader1800::read_tileset(BinaryFile& mapfile)
{
    //Load tileset information

    short iNumTilesets = (short)mapfile.read_i32();

    TilesetTranslation * translation = new TilesetTranslation[iNumTilesets];

    iMaxTilesetID = 0; //Figure out how big the translation array needs to be
    for (short iTileset = 0; iTileset < iNumTilesets; iTileset++) {
        short iTilesetID = mapfile.read_i32();
        translation[iTileset].iID = iTilesetID;

        if (iTilesetID > iMaxTilesetID)
            iMaxTilesetID = iTilesetID;

        mapfile.read_string_long(translation[iTileset].szName, 128);
    }

    translationid = new short[iMaxTilesetID + 1];
    tilesetwidths = new short[iMaxTilesetID + 1];
    tilesetheights = new short[iMaxTilesetID + 1];

    for (short iTileset = 0; iTileset < iNumTilesets; iTileset++) {
        short iID = translation[iTileset].iID;
        translationid[iID] = g_tilesetmanager->indexFromName(translation[iTileset].szName);

        if (translationid[iID] == TILESETUNKNOWN) {
            tilesetwidths[iID] = 1;
            tilesetheights[iID] = 1;
        } else {
            tilesetwidths[iID] = g_tilesetmanager->tileset(translationid[iID])->width();
            tilesetheights[iID] = g_tilesetmanager->tileset(translationid[iID])->height();
        }
    }

    delete [] translation;
}

void MapReader1800::read_tiles(CMap& map, BinaryFile& mapfile)
{
    //2. load map data

    unsigned short i, j, k;
    for (j = 0; j < MAPHEIGHT; j++) {
        for (i = 0; i < MAPWIDTH; i++) {
            for (k = 0; k < MAPLAYERS; k++) {
                TilesetTile * tile = &map.mapdata[i][j][k];
                tile->iID = mapfile.read_i8();
                tile->iCol = mapfile.read_i8();
                tile->iRow = mapfile.read_i8();

                if (tile->iID >= 0) {
                    if (tile->iID > iMaxTilesetID)
                        tile->iID = 0;

                    //Make sure the column and row we read in is within the bounds of the tileset
                    if (tile->iCol < 0 || tile->iCol >= tilesetwidths[tile->iID])
                        tile->iCol = 0;

                    if (tile->iRow < 0 || tile->iRow >= tilesetheights[tile->iID])
                        tile->iRow = 0;

                    //Convert tileset ids into the current game's tileset's ids
                    tile->iID = translationid[tile->iID];
                }
            }

            map.objectdata[i][j].iType = mapfile.read_i8();
            map.objectdata[i][j].fHidden = mapfile.read_bool();
        }
    }
}

void MapReader1800::read_switches(CMap& map, BinaryFile& mapfile)
{
    // Read the on/off switch state of the four colors (turned on or off)
    for (short iSwitch = 0; iSwitch < 4; iSwitch++)
        map.iSwitches[iSwitch] = (short)mapfile.read_i32();
}

void MapReader1800::read_items(CMap& map, BinaryFile& mapfile)
{
    //Load map items (like carryable spikes and springs)
    const size_t iNumMapItems = mapfile.read_i32();

    for (size_t idx = 0; idx < iNumMapItems; idx++) {
        MapItem item = {};
        item.itype = static_cast<MapItemType>(mapfile.read_i32()); // TODO: Error handling
        item.ix = mapfile.read_i32();
        item.iy = mapfile.read_i32();
        map.mapitems.emplace_back(std::move(item));
    }
}

void MapReader1800::read_hazards(CMap& map, BinaryFile& mapfile)
{
    //Load map hazards (like fireball strings, rotodiscs, pirhana plants)
    const size_t iNumMapHazards = mapfile.read_i32();

    for (size_t idx = 0; idx < iNumMapHazards; idx++) {
        MapHazard hazard = {};
        hazard.itype = mapfile.read_i32();
        hazard.ix = mapfile.read_i32();
        hazard.iy = mapfile.read_i32();

        for (short iParam = 0; iParam < NUMMAPHAZARDPARAMS; iParam++)
            hazard.iparam[iParam] = mapfile.read_i32();

        for (short iParam = 0; iParam < NUMMAPHAZARDPARAMS; iParam++)
            hazard.dparam[iParam] = mapfile.read_float();

        map.maphazards.emplace_back(std::move(hazard));
    }
}

void MapReader1802::read_eyecandy(CMap& map, BinaryFile& mapfile)
{
    //Read in eyecandy to use
    //For all layers if the map format supports it
    map.eyecandy[0] = (short)mapfile.read_i32();
    map.eyecandy[1] = (short)mapfile.read_i32();
    map.eyecandy[2] = (short)mapfile.read_i32();
}

void MapReader1800::read_warp_locations(CMap& map, BinaryFile& mapfile)
{
    for (unsigned short j = 0; j < MAPHEIGHT; j++) {
        for (unsigned short i = 0; i < MAPWIDTH; i++) {
            map.mapdatatop[i][j] = (TileType)mapfile.read_i32();;

            map.warpdata[i][j].direction = (WarpEnterDirection)mapfile.read_i32();
            map.warpdata[i][j].connection = (short)mapfile.read_i32();
            map.warpdata[i][j].id = (short)mapfile.read_i32();

            for (short sType = 0; sType < NUMSPAWNAREATYPES; sType++)
                map.nospawn[sType][i][j] = mapfile.read_bool();

        }
    }
}

void MapReader1800::read_switchable_blocks(CMap& map, BinaryFile& mapfile)
{
    //Read switch block state data
    int iNumSwitchBlockData = mapfile.read_i32();
    for (short iBlock = 0; iBlock < iNumSwitchBlockData; iBlock++) {
        short iCol = mapfile.read_i8();
        short iRow = mapfile.read_i8();

        map.objectdata[iCol][iRow].iSettings[0] = mapfile.read_i8();
    }
}

bool MapReader1800::read_spawn_areas(CMap& map, BinaryFile& mapfile)
{
    for (unsigned short i = 0; i < NUMSPAWNAREATYPES; i++) {
        map.totalspawnsize[i] = 0;
        map.numspawnareas[i] = (short)mapfile.read_i32();

        if (map.numspawnareas[i] > MAXSPAWNAREAS) {
            std::cout << std::endl << " ERROR: Number of spawn areas (" << map.numspawnareas[i]
                << ") was greater than max allowed (" << MAXSPAWNAREAS << ')'
                << std::endl;
            return false;
        }

        for (int m = 0; m < map.numspawnareas[i]; m++) {
            map.spawnareas[i][m].left = (short)mapfile.read_i32();
            map.spawnareas[i][m].top = (short)mapfile.read_i32();
            map.spawnareas[i][m].width = (short)mapfile.read_i32();
            map.spawnareas[i][m].height = (short)mapfile.read_i32();
            map.spawnareas[i][m].size = (short)mapfile.read_i32();

            map.totalspawnsize[i] += map.spawnareas[i][m].size;
        }

        //If no spawn areas were identified, then create one big spawn area
        if (map.totalspawnsize[i] == 0) {
            map.numspawnareas[i] = 1;
            map.spawnareas[i][0].left = 0;
            map.spawnareas[i][0].width = 20;
            map.spawnareas[i][0].top = 1;
            map.spawnareas[i][0].height = 12;
            map.spawnareas[i][0].size = 220;
            map.totalspawnsize[i] = 220;
        }
    }

    return true;
}

void MapReader1800::read_extra_tiledata(CMap& map, BinaryFile& mapfile)
{
    int iNumExtendedDataBlocks = mapfile.read_i32();

    for (short iBlock = 0; iBlock < iNumExtendedDataBlocks; iBlock++) {
        short iCol = mapfile.read_i8();
        short iRow = mapfile.read_i8();

        short iNumSettings = mapfile.read_i8();
        for (short iSetting = 0; iSetting < iNumSettings; iSetting++)
            map.objectdata[iCol][iRow].iSettings[iSetting] = mapfile.read_i8();
    }
}

void MapReader1800::read_gamemode_settings(CMap& map, BinaryFile& mapfile)
{
    //read mode item locations like flags and race goals
    map.iNumRaceGoals = (short)mapfile.read_i32();
    for (unsigned short j = 0; j < map.iNumRaceGoals; j++) {
        map.racegoallocations[j].x = (short)mapfile.read_i32();
        map.racegoallocations[j].y = (short)mapfile.read_i32();
    }

    map.iNumFlagBases = (short)mapfile.read_i32();
    for (unsigned short j = 0; j < map.iNumFlagBases; j++) {
        map.flagbaselocations[j].x = (short)mapfile.read_i32();
        map.flagbaselocations[j].y = (short)mapfile.read_i32();
    }
}

void MapReader1800::read_platforms(CMap& map, BinaryFile& mapfile, bool fPreview)
{
    map.clearPlatforms();

    //Load moving platforms
    const size_t iNumPlatforms = (short)mapfile.read_i32();
    map.platforms.reserve(iNumPlatforms);

    for (size_t idx = 0; idx < iNumPlatforms; idx++) {
        short iWidth = (short)mapfile.read_i32();
        short iHeight = (short)mapfile.read_i32();

        auto [tiles, types] = read_platform_tiles(map, mapfile, iWidth, iHeight);

        short iDrawLayer = 2;
        if (patch_version >= 1)
            iDrawLayer = (short)mapfile.read_i32();

        //printf("Layer: %d\n", iDrawLayer);

        short iPathType = 0;
        iPathType = mapfile.read_i32();

        //printf("PathType: %d\n", iPathType);

        MovingPlatformPath* path = NULL;
        path = read_platform_path_details(mapfile, iPathType, fPreview);
        if (!path)
            continue;

        MovingPlatform* platform = new MovingPlatform(std::move(tiles), std::move(types), iWidth, iHeight, iDrawLayer, path, fPreview);
        map.platforms.emplace_back(platform);
        map.platformdrawlayer[iDrawLayer].push_back(platform);
    }
}

std::pair<std::vector<TilesetTile>, std::vector<TileType>>
MapReader1800::read_platform_tiles(CMap& map, BinaryFile& mapfile, short iWidth, short iHeight)
{
    std::vector<TilesetTile> tiles;
    std::vector<TileType> types;
    tiles.reserve(iWidth * iHeight);
    types.reserve(iWidth * iHeight);

    for (short iCol = 0; iCol < iWidth; iCol++) {
        for (short iRow = 0; iRow < iHeight; iRow++) {
            TilesetTile tile;

            tile.iID = mapfile.read_i8();
            tile.iCol = mapfile.read_i8();
            tile.iRow = mapfile.read_i8();

            if (tile.iID >= 0) {
                if (iMaxTilesetID != -1 && tile.iID > iMaxTilesetID)
                    tile.iID = 0;

                //Make sure the column and row we read in is within the bounds of the tileset
                if (tile.iCol < 0 || (tilesetwidths && tile.iCol >= tilesetwidths[tile.iID]))
                    tile.iCol = 0;

                if (tile.iRow < 0 || (tilesetheights && tile.iRow >= tilesetheights[tile.iID]))
                    tile.iRow = 0;

                //Convert tileset ids into the current game's tileset's ids
                if (translationid)
                    tile.iID = translationid[tile.iID];
            }

            TileType type = static_cast<TileType>(mapfile.read_i32());

            tiles.emplace_back(std::move(tile));
            types.emplace_back(std::move(type));
        }
    }

    return {tiles, types};
}

bool MapReader1800::load(CMap& map, BinaryFile& mapfile/*, const char* filename*/, ReadType readtype)
{
    read_autofilters(map, mapfile);

    if (readtype == read_type_summary)
        return true;

    map.clearPlatforms();

    // FIXME
    /*cout << "loading map " << filename;

    if (readtype == read_type_preview)
        cout << " (preview)";

    cout << " [Version " << version[0] << '.' << version[1] << '.'
         << version[2] << '.' << version[3] << " Map Detected]\n";*/

    read_tileset(mapfile);

    read_tiles(map, mapfile);
    read_background(map, mapfile);
    read_switches(map, mapfile);
    read_platforms(map, mapfile, readtype == read_type_preview);

    //All tiles have been loaded so the translation is no longer needed
    delete [] translationid;
    delete [] tilesetwidths;
    delete [] tilesetheights;

    read_items(map, mapfile);
    read_hazards(map, mapfile);
    read_eyecandy(map, mapfile);
    read_music_category(map, mapfile);
    read_warp_locations(map, mapfile);
    read_switchable_blocks(map, mapfile);

    if (readtype == read_type_preview)
        return true;

    read_warp_exits(map, mapfile);
    if (!read_spawn_areas(map, mapfile))
        return false;

    if (!read_draw_areas(map, mapfile))
        return false;

    read_extra_tiledata(map, mapfile);
    read_gamemode_settings(map, mapfile);

    return true;
}
