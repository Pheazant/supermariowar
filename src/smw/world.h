#ifndef WORLD_H
#define WORLD_H

#include "SDL.h"

#include <string>
#include <vector>

#define WORLD_BACKGROUND_SPRITE_SET_SIZE    60
#define WORLD_PATH_SPRITE_SET_SIZE          20

#define WORLD_FOREGROUND_STAGE_OFFSET           200
#define WORLD_WINNING_TEAM_SPRITE_OFFSET        600
#define WORLD_BRIDGE_SPRITE_OFFSET              604
#define WORLD_START_SPRITE_OFFSET               608
#define WORLD_FOREGROUND_SPRITE_OFFSET          700
#define WORLD_FOREGROUND_SPRITE_ANIMATED_OFFSET 900

struct WorldMapTile {
	//Id is used for searching for AI
	short iID;

	short iType;
	short iBackgroundWater;
	short iBackgroundSprite;
	short iForegroundSprite;

	short iConnectionType;
	bool fConnection[4];

	short iWarp;

	short iCompleted;
	bool fAnimated;

	short iCol;
	short iRow;

	short iVehicleBoundary;
};

class WorldMovingObject
{
	public:
		WorldMovingObject();
		virtual ~WorldMovingObject();

		void Init(short iCol, short iRow, short iSprite, short iInitialDirection, short tilesize);
		virtual void Move(short iDirection);
		virtual bool Update();
		void Draw(short iWorldOffsetX, short iWorldOffsetY);
		void FaceDirection(short iDirection);
		void SetPosition(short iCol, short iRow);

	protected:

		short ix;
		short iy;
		short iCurrentTileX;
		short iCurrentTileY;
		short iDestTileX;
		short iDestTileY;

		short iState;
		short iDrawSprite;
		short iDrawDirection;
		short iAnimationFrame;
		short iAnimationTimer;

		short iTileSize;
		short iTileSheet;

	friend class WorldMap;
	friend void takescreenshot();
};

class WorldPlayer : public WorldMovingObject
{
	public:

		WorldPlayer();
		~WorldPlayer();

		void Init(short iCol, short iRow);

		void SetSprite(short iPlayer);
                void Draw(short iWorldOffsetX, short iWorldOffsetY) const;

	friend class WorldMap;
};

class WorldVehicle : public WorldMovingObject
{
	public:

		WorldVehicle();
		~WorldVehicle();

		void Init(short iCol, short iRow, short iAction, short iSprite, short iMinMoves, short iMaxMoves, bool fSpritePaces, short iInitialDirection, short iBoundary, short tilesize);
		void Move();

		bool Update();
                void Draw(short iWorldOffsetX, short iWorldOffsetY, bool fVehiclesSleeping) const;

	private:

		void SetNextDest();

		SDL_Rect srcRects[5];

		short iMinMoves;
		short iMaxMoves;
		short iNumMoves;

		short iActionId;

		bool fEnabled;

		bool fSpritePaces;
		short iPaceOffset;
		short iPaceTimer;

		short iBoundary;

	friend class WorldMap;
	friend void AddVehicleToTile(short iCol, short iRow, short iType);
	friend void RemoveVehicleFromTile(short iCol, short iRow);
	friend void ReadVehiclesIntoEditor();
	friend void WriteVehiclesIntoWorld();

	friend int editor_stage();
	friend int editor_edit();
	friend int resize_world();
	friend int editor_vehicles();
	friend int main(int argc, char *argv[]);
};

class WorldWarp
{
	public:
		WorldWarp();
		void Init(short id, short col1, short row1, short col2, short row2);
                void GetOtherSide(short iCol, short iRow, short * iOtherCol, short * iOtherRow) const;

	private:
		short iCol1, iRow1;
		short iCol2, iRow2;
		short iID;

	friend class WorldMap;
	friend void AddWarpToTile(short iCol, short iRow, short iType);
	friend void RemoveWarpFromTile(short iCol, short iRow);
	friend void ReadWarpsIntoEditor();
	friend void WriteWarpsIntoWorld();
	friend int editor_edit();
	friend void takescreenshot();
};

class WorldMap {
public:
    WorldMap();
    ~WorldMap();

    bool Load(short iTileSize);
    bool Save() const;
    bool Save(const std::string& szPath) const;

    void New(short iWidth, short iHeight);
    void Resize(short iWidth, short iHeight);
    void Clear();

    void InitPlayer();

    bool Update(bool* fPlayerVehicleCollision);
    void Draw(short iMapOffsetX, short iMapOffsetY, bool fDrawPlayer, bool fVehiclesSleeping) const;

    void UpdateTile(SDL_Surface* surface, short iCol, short iRow, short iMapDrawOffsetCol, short iMapDrawOffsetRow, short iAnimationFrame);

    void ResetDrawCycle();
    void DrawMapToSurface(SDL_Surface* surface) const;
    void DrawMapToSurface(short iCycleIndex, bool fFullRefresh, SDL_Surface* surface, short iMapDrawOffsetCol, short iMapDrawOffsetRow, short iAnimationFrame);

    void SetPlayerSprite(short iPlayerSprite);
    bool IsVehicleMoving() const;

    void GetWorldSize(short * w, short * h) const {
        *w = iWidth;
        *h = iHeight;
    }

    void GetPlayerPosition(short * iPlayerX, short * iPlayerY) const;
    void SetPlayerPosition(short iPlayerCol, short iPlayerRow);

    void GetPlayerCurrentTile(short * iPlayerCurrentTileX, short * iPlayerCurrentTileY) const;
    void GetPlayerDestTile(short * iPlayerDestTileX, short * iPlayerDestTileY) const;

    short GetPlayerState() const;

    short GetVehicleInPlayerTile(short * iVehicleIndex) const;
    bool GetWarpInPlayerTile(short * iWarpCol, short * iWarpRow) const;

    void MovePlayer(short iDirection);
    void FacePlayer(short iDirection);
    void MoveVehicles();

    void RemoveVehicle(short iVehicleIndex);

    short NumVehiclesInTile(short iTileX, short iTileY) const;

    short GetVehicleStageScore(short iVehicleIndex) const;
    void MoveBridges();

    void IsTouchingDoor(short iCol, short iRow, bool doors[4]) const;
    bool IsDoor(short iCol, short iRow) const;
    short UseKey(short iKeytype, short iCol, short iRow, bool fCloud);

    short GetVehicleBoundary(short iCol, short iRow) const;

    short GetNextInterestingMove(short iCol, short iRow) const;

    void SetInitialPowerups();

    short GetMusicCategory() const { return iMusicCategory; }
    const std::string& GetWorldName() { return worldName; }

	private:

		void Cleanup();
		void SetTileConnections(short iCol, short iRow);

		void DrawTileToSurface(SDL_Surface* surface, short iCol, short iRow, short iMapDrawOffsetCol, short iMapDrawOffsetRow, bool fFullRefresh, short iAnimationFrame, short iLayer = 0) const;

		short iWidth = 0;
		short iHeight = 0;
                short iStartX = 0;
                short iStartY = 0;

		short iNumStages = 0;

                std::vector<std::vector<WorldMapTile>> tiles;
		WorldPlayer player;
                std::vector<WorldVehicle> vehicles;
                std::vector<WorldWarp> warps;

		short iNumInitialBonuses;
		short iInitialBonuses[32];

		short iMusicCategory;

		short iTileSize;
		short iTileSizeShift;
		short iTileSheet;

		short iLastDrawRow;
		short iLastDrawCol;

		short iTilesPerCycle;

		std::string worldName;

	friend class MI_World;
	friend class MI_WorldPreviewDisplay;
	friend class WorldVehicle;

	friend int editor_edit();
	friend int editor_type();
	friend int editor_stage();
	friend int editor_start_items();
	friend int editor_vehicles();
	friend void NewStage(short * iEditStage);

	friend bool AutoSetTile(short iCol, short iRow);
	friend void AutoSetPath(short iCol, short iRow);
	friend void AutoSetPathSprite(short iCol, short iRow);
	friend short AdjustForeground(short iSprite, short iCol, short iRow);
	friend bool UpdateForeground(short iCol, short iRow);

	friend void GetForegroundTileValues(short iCol, short iRow, short iOldTiles[9]);
	friend bool ForegroundTileValuesChanged(short iCol, short iRow, short iOldTiles[9]);

	friend void ReadVehiclesIntoEditor();
	friend void WriteVehiclesIntoWorld();
	friend void ReadWarpsIntoEditor();
	friend void WriteWarpsIntoWorld();
};

#endif // WORLD_H
