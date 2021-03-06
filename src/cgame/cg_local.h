/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2018 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "game/bg_public.h"
#include "qcommon/cvar.h"
#include "qcommon/cmd.h"
#include "qcommon/files.h"
#include "qcommon/q_shared.h"
#include "renderercommon/tr_public.h"
#include "renderercommon/tr_types.h"
#include "ui/ui_shared.h"
#include "sys/sys_shared.h"

#include "cg_public.h"
#include "binaryshader.h"
// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#define CG_FONT_THRESHOLD 0.1

#define POWERUP_BLINKS      5

#define POWERUP_BLINK_TIME  1000
#define FADE_TIME           200
#define PULSE_TIME          200
#define DAMAGE_DEFLECT_TIME 100
#define DAMAGE_RETURN_TIME  400
#define DAMAGE_TIME         500
#define LAND_DEFLECT_TIME   150
#define LAND_RETURN_TIME    300
#define DUCK_TIME           100
#define PAIN_TWITCH_TIME    200
#define WEAPON_SELECT_TIME  1400
#define ITEM_SCALEUP_TIME   1000
#define ZOOM_TIME           150
#define ITEM_BLOB_TIME      200
#define MUZZLE_FLASH_TIME   20
#define SINK_TIME           1000    // time for fragments to sink into ground before going away
#define ATTACKER_HEAD_TIME  10000
#define REWARD_TIME         3000

#define PULSE_SCALE         1.5     // amount to scale up the icons when activating

#define MAX_STEP_CHANGE     32

#define MAX_VERTS_ON_POLY   10
#define MAX_MARK_POLYS      256

#define STAT_MINUS          10  // num frame for '-' stats digit

#define ICON_SIZE           48
#define CHAR_WIDTH          32
#define CHAR_HEIGHT         48
#define TEXT_ICON_SPACE     4

#define TEAMCHAT_WIDTH     80
#define TEAMCHAT_HEIGHT     8

// very large characters
#define GIANT_WIDTH         32
#define GIANT_HEIGHT        48

#define NUM_CROSSHAIRS      10

#define TEAM_OVERLAY_MAXNAME_WIDTH  12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH  16

enum footstep_t
{
  FOOTSTEP_NORMAL,
  FOOTSTEP_FLESH,
  FOOTSTEP_METAL,
  FOOTSTEP_SPLASH,
  FOOTSTEP_CUSTOM,
  FOOTSTEP_NONE,

  FOOTSTEP_TOTAL
};

enum impactSound_t
{
  IMPACTSOUND_DEFAULT,
  IMPACTSOUND_METAL,
  IMPACTSOUND_FLESH
};

enum jetPackState_t
{
  JPS_OFF,
  JPS_DESCENDING,
  JPS_HOVERING,
  JPS_ASCENDING
};

//======================================================================

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
struct lerpFrame_t
{
  int         oldFrame;
  int         oldFrameTime;     // time when ->oldFrame was exactly on

  int         frame;
  int         frameTime;        // time when ->frame will be exactly on

  float       backlerp;

  float       yawAngle;
  bool    yawing;
  float       pitchAngle;
  bool    pitching;

  int         animationNumber;  // may include ANIM_TOGGLEBIT
  animation_t *animation;
  int         animationTime;    // time when the first frame of the animation will be exact
};

//======================================================================

//attachment system
enum attachmentType_t
{
  AT_STATIC,
  AT_TAG,
  AT_CENT,
  AT_PARTICLE
};

//forward declaration for particle_t
struct particle_t;

struct attachment_t
{
  attachmentType_t  type;
  bool          attached;

  bool          staticValid;
  bool          tagValid;
  bool          centValid;
  bool          particleValid;

  bool          hasOffset;
  vec3_t            offset;

  vec3_t            lastValidAttachmentPoint;

  //AT_STATIC
  vec3_t            origin;

  //AT_TAG
  refEntity_t       re;     //FIXME: should be pointers?
  refEntity_t       parent; //
  qhandle_t         model;
  char              tagName[ MAX_STRING_CHARS ];

  //AT_CENT
  int               centNum;

  //AT_PARTICLE
  particle_t *particle;
};

//======================================================================

//particle system stuff
#define MAX_PARTICLE_FILES        128

#define MAX_PS_SHADER_FRAMES      32
#define MAX_PS_MODELS             8
#define MAX_EJECTORS_PER_SYSTEM   4
#define MAX_PARTICLES_PER_EJECTOR 4

#define MAX_BASEPARTICLE_SYSTEMS  192
#define MAX_BASEPARTICLE_EJECTORS MAX_BASEPARTICLE_SYSTEMS*MAX_EJECTORS_PER_SYSTEM
#define MAX_BASEPARTICLES         MAX_BASEPARTICLE_EJECTORS*MAX_PARTICLES_PER_EJECTOR

#define MAX_PARTICLE_SYSTEMS      48
#define MAX_PARTICLE_EJECTORS     MAX_PARTICLE_SYSTEMS*MAX_EJECTORS_PER_SYSTEM
#define MAX_PARTICLES             MAX_PARTICLE_EJECTORS*5

#define PARTICLES_INFINITE        -1
#define PARTICLES_SAME_AS_INITIAL -2

//COMPILE TIME STRUCTURES
enum pMoveType_t
{
  PMT_STATIC,
  PMT_STATIC_TRANSFORM,
  PMT_TAG,
  PMT_CENT_ANGLES,
  PMT_NORMAL,
  PMT_LAST_NORMAL,
  PMT_OPPORTUNISTIC_NORMAL
};

enum pDirType_t
{
  PMD_LINEAR,
  PMD_POINT
};

struct pMoveValues_t
{
  pDirType_t  dirType;

  //PMD_LINEAR
  vec3_t      dir;
  float       dirRandAngle;

  //PMD_POINT
  vec3_t      point;
  float       pointRandAngle;

  float       mag;
  float       magRandFrac;

  float       parentVelFrac;
  float       parentVelFracRandFrac;
};

struct pLerpValues_t
{
  int   delay;
  float delayRandFrac;

  float initial;
  float initialRandFrac;

  float final;
  float finalRandFrac;

  float randFrac;
};

//particle template
struct baseParticle_t
{
  vec3_t          displacement;
  vec3_t          randDisplacement;
  float           normalDisplacement;

  pMoveType_t     velMoveType;
  pMoveValues_t   velMoveValues;

  pMoveType_t     accMoveType;
  pMoveValues_t   accMoveValues;

  int             lifeTime;
  float           lifeTimeRandFrac;

  float           bounceFrac;
  float           bounceFracRandFrac;
  bool        bounceCull;

  char            bounceMarkName[ MAX_QPATH ];
  qhandle_t       bounceMark;
  float           bounceMarkRadius;
  float           bounceMarkRadiusRandFrac;
  float           bounceMarkCount;
  float           bounceMarkCountRandFrac;

  char            bounceSoundName[ MAX_QPATH ];
  qhandle_t       bounceSound;
  float           bounceSoundCount;
  float           bounceSoundCountRandFrac;

  pLerpValues_t   radius;
  int             physicsRadius;
  pLerpValues_t   alpha;
  pLerpValues_t   rotation;

  bool        dynamicLight;
  pLerpValues_t   dLightRadius;
  byte            dLightColor[ 3 ];

  int             colorDelay;
  float           colorDelayRandFrac;
  byte            initialColor[ 3 ];
  byte            finalColor[ 3 ];

  char            childSystemName[ MAX_QPATH ];
  qhandle_t       childSystemHandle;

  char            onDeathSystemName[ MAX_QPATH ];
  qhandle_t       onDeathSystemHandle;

  char            childTrailSystemName[ MAX_QPATH ];
  qhandle_t       childTrailSystemHandle;

  //particle invariant stuff
  char            shaderNames[ MAX_PS_SHADER_FRAMES ][ MAX_QPATH ];
  qhandle_t       shaders[ MAX_PS_SHADER_FRAMES ];
  int             numFrames;
  float           framerate;

  char            modelNames[ MAX_PS_MODELS ][ MAX_QPATH ];
  qhandle_t       models[ MAX_PS_MODELS ];
  int             numModels;
  animation_t     modelAnimation;

  bool        overdrawProtection;
  bool        realLight;
  bool        cullOnStartSolid;
  
  float           scaleWithCharge;
};


//ejector template
struct baseParticleEjector_t
{
  baseParticle_t  *particles[ MAX_PARTICLES_PER_EJECTOR ];
  int             numParticles;

  pLerpValues_t   eject;          //zero period indicates creation of all particles at once

  int             totalParticles;         //can be infinite
  float           totalParticlesRandFrac;
};


//particle system template
struct baseParticleSystem_t
{
  char                  name[ MAX_QPATH ];
  baseParticleEjector_t *ejectors[ MAX_EJECTORS_PER_SYSTEM ];
  int                   numEjectors;

  bool              thirdPersonOnly;
  bool              registered; //whether or not the assets for this particle have been loaded
};


//RUN TIME STRUCTURES
struct particleSystem_t
{
  baseParticleSystem_t  *klass;

  attachment_t          attachment;

  bool              valid;
  bool              lazyRemove; //mark this system for later removal

  //for PMT_NORMAL
  bool              normalValid;
  vec3_t                normal;
  //for PMT_LAST_NORMAL and PMT_OPPORTUNISTIC_NORMAL
  bool              lastNormalIsCurrent;
  vec3_t                lastNormal;
  
  int                   charge;
};


struct particleEjector_t
{
  baseParticleEjector_t *klass;
  particleSystem_t      *parent;

  pLerpValues_t         ejectPeriod;

  int                   count;
  int                   totalParticles;

  int                   nextEjectionTime;

  bool              valid;
};


//used for actual particle evaluation
struct particle_t
{
  baseParticle_t    *klass;
  particleEjector_t *parent;

  particleSystem_t  *childParticleSystem;

  int               birthTime;
  int               lifeTime;

  float             bounceMarkRadius;
  int               bounceMarkCount;
  int               bounceSoundCount;
  bool          atRest;

  vec3_t            origin;
  vec3_t            velocity;

  pMoveType_t       accMoveType;
  pMoveValues_t     accMoveValues;

  int               lastEvalTime;

  int               nextChildTime;

  pLerpValues_t     radius;
  pLerpValues_t     alpha;
  pLerpValues_t     rotation;

  pLerpValues_t     dLightRadius;

  int               colorDelay;

  qhandle_t         model;
  lerpFrame_t       lf;
  vec3_t            lastAxis[ 3 ];

  bool          valid;
  int               frameWhenInvalidated;

  int               sortKey;
};

//======================================================================

//trail system stuff
#define MAX_TRAIL_FILES           128

#define MAX_BEAMS_PER_SYSTEM      4

#define MAX_BASETRAIL_SYSTEMS     64
#define MAX_BASETRAIL_BEAMS       MAX_BASETRAIL_SYSTEMS*MAX_BEAMS_PER_SYSTEM

#define MAX_TRAIL_SYSTEMS         32
#define MAX_TRAIL_BEAMS           MAX_TRAIL_SYSTEMS*MAX_BEAMS_PER_SYSTEM
#define MAX_TRAIL_BEAM_NODES      128

#define MAX_TRAIL_BEAM_JITTERS    4

enum trailBeamTextureType_t
{
  TBTT_STRETCH,
  TBTT_REPEAT
};

struct baseTrailJitter_t
{
  float   magnitude;
  int     period;
};

//beam template
struct baseTrailBeam_t
{
  int                     numSegments;
  float                   frontWidth;
  float                   backWidth;
  float                   frontAlpha;
  float                   backAlpha;
  byte                    frontColor[ 3 ];
  byte                    backColor[ 3 ];

  // the time it takes for a segment to vanish (single attached only)
  int                     segmentTime;

  // the time it takes for a beam to fade out (double attached only)
  int                     fadeOutTime;
  
  char                    shaderName[ MAX_QPATH ];
  qhandle_t               shader;

  trailBeamTextureType_t  textureType;

  //TBTT_STRETCH
  float                   frontTextureCoord;
  float                   backTextureCoord;

  //TBTT_REPEAT
  float                   repeatLength;
  bool                clampToBack;

  bool                realLight;

  int                     numJitters;
  baseTrailJitter_t       jitters[ MAX_TRAIL_BEAM_JITTERS ];
  bool                jitterAttachments;
};


//trail system template
struct baseTrailSystem_t
{
  char            name[ MAX_QPATH ];
  baseTrailBeam_t *beams[ MAX_BEAMS_PER_SYSTEM ];
  int             numBeams;

  int             lifeTime;
  bool        thirdPersonOnly;
  bool        registered; //whether or not the assets for this trail have been loaded
};

struct trailSystem_t
{
  baseTrailSystem_t   *klass;

  attachment_t        frontAttachment;
  attachment_t        backAttachment;

  int                 birthTime;
  int                 destroyTime;
  bool            valid;
};

struct trailBeamNode_t
{
  vec3_t                  refPosition;
  vec3_t                  position;

  int                     timeLeft;

  float                   textureCoord;
  float                   halfWidth;
  byte                    alpha;
  byte                    color[ 3 ];

  vec2_t                  jitters[ MAX_TRAIL_BEAM_JITTERS ];

  trailBeamNode_t  *prev;
  trailBeamNode_t  *next;

  bool                used;
};

struct trailBeam_t
{
  baseTrailBeam_t   *klass;
  trailSystem_t     *parent;

  trailBeamNode_t   nodePool[ MAX_TRAIL_BEAM_NODES ];
  trailBeamNode_t   *nodes;

  int               lastEvalTime;

  bool          valid;

  int               nextJitterTimes[ MAX_TRAIL_BEAM_JITTERS ];
};

//======================================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// smoothing of view and model for WW transitions
#define   MAXSMOOTHS          32

struct smooth_t
{
  float     time;
  float     timeMod;

  vec3_t    rotAxis;
  float     rotAngle;
};


struct playerEntity_t
{
  lerpFrame_t legs;
  lerpFrame_t torso;
  lerpFrame_t nonseg;
  lerpFrame_t weapon;

  int         painTime;
  int         painDirection;  // flip from 0 to 1

  // machinegun spinning
  float       barrelAngle;
  int         barrelTime;
  bool    barrelSpinning;

  vec3_t      lastNormal;
  vec3_t      lastAxis[ 3 ];
  smooth_t    sList[ MAXSMOOTHS ];
};

struct lightFlareStatus_t
{
  float     lastRadius;    //caching of likely flare radius
  float     lastRatio;     //caching of likely flare ratio
  int       lastTime;      //last time flare was visible/occluded
  bool  status;        //flare is visble?
};

struct buildableStatus_t
{
  int       lastTime;      // Last time status was visible
  bool  visible;       // Status is visble?
};

struct buildableCache_t
{
  vec3_t   cachedOrigin;   // If any of the values differ from their
  vec3_t   cachedAngles;   //  cached versions, then the cache is invalid
  vec3_t   cachedNormal;
  buildable_t cachedType;
  vec3_t   axis[ 3 ];
  vec3_t   origin;
};

//=================================================

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
struct centity_t
{
  entityState_t         currentState;     // from cg.frame
  entityState_t         nextState;        // from cg.nextFrame, if available
  bool              interpolate;      // true if next is valid to interpolate to
  bool              currentValid;     // true if cg.frame holds this entity

  int                   muzzleFlashTime;  // move to playerEntity?
  int                   muzzleFlashTime2; // move to playerEntity?
  int                   muzzleFlashTime3; // move to playerEntity?
  int                   previousEvent;
  int                   teleportFlag;

  int                   trailTime;        // so missile trails can handle dropped initial packets
  int                   dustTrailTime;
  int                   miscTime;
  int                   snapShotTime;     // last time this entity was found in a snapshot

  playerEntity_t        pe;

  int                   errorTime;        // decay the error from this time
  vec3_t                errorOrigin;
  vec3_t                errorAngles;

  bool              extrapolated;     // false if origin / angles is an interpolation
  vec3_t                rawOrigin;
  vec3_t                rawAngles;

  vec3_t                beamEnd;

  // exact interpolated position of entity on this frame
  vec3_t                lerpOrigin;
  vec3_t                lerpAngles;

  lerpFrame_t           lerpFrame;

  buildableAnimNumber_t buildableAnim;    //persistant anim number
  buildableAnimNumber_t oldBuildableAnim; //to detect when new anims are set
  bool              buildableIdleAnim; //to check if new idle anim
  particleSystem_t      *buildablePS;
  buildableStatus_t     buildableStatus;
  buildableCache_t      buildableCache;   // so we don't recalculate things
  float                 lastBuildableHealth;
  int                   lastBuildableDamageSoundTime;

  lightFlareStatus_t    lfs;

  bool              doorState;

  bool              animInit;
  bool              animPlaying;
  bool              animLastState;

  particleSystem_t      *muzzlePS;
  bool              muzzlePsTrigger;

  particleSystem_t      *jetPackPS;
  jetPackState_t        jetPackState;

  particleSystem_t      *poisonCloudedPS;

  particleSystem_t      *entityPS;
  bool              entityPSMissing;

  trailSystem_t         *level2ZapTS[ LEVEL2_AREAZAP_MAX_TARGETS ];
  int                   level2ZapTime;

  trailSystem_t         *muzzleTS; //used for the tesla and reactor
  int                   muzzleTSDeathTime;

  bool              valid;
  bool              oldValid;  
  centity_t      *nextLocation;
};


//======================================================================

struct markPoly_t
{
  markPoly_t *prevMark;
  markPoly_t *nextMark;
  int               time;
  qhandle_t         markShader;
  bool          alphaFade;    // fade alpha instead of rgb
  float             color[ 4 ];
  poly_t            poly;
  polyVert_t        verts[ MAX_VERTS_ON_POLY ];
};

//======================================================================


struct score_t
{
  int       client;
  int       score;
  int       ping;
  int       time;
  int       team;
  weapon_t  weapon;
  upgrade_t upgrade;
};

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define MAX_CUSTOM_SOUNDS 32
struct clientInfo_t
{
  bool    infoValid;

  char        name[ MAX_NAME_LENGTH ];
  team_t      team;

  int         score;                      // updated by score servercmds
  int         location;                   // location index for team mode
  int         health;                     // you only get this info about your teammates
  int         upgrade; 
  int         curWeaponClass;             // sends current weapon for H, current klass for A

  // when clientinfo is changed, the loading of models/skins/sounds
  // can be deferred until you are dead, to prevent hitches in
  // gameplay
  char        modelName[ MAX_QPATH ];
  char        skinName[ MAX_QPATH ];

  bool    newAnims;                   // true if using the new mission pack animations
  bool    fixedlegs;                  // true if legs yaw is always the same as torso yaw
  bool    fixedtorso;                 // true if torso never changes yaw
  bool    nonsegmented;               // true if model is Q2 style nonsegmented

  vec3_t      headOffset;                 // move head in icon views
  footstep_t  footsteps;
  gender_t    gender;                     // from model

  qhandle_t   legsModel;
  qhandle_t   legsSkin;

  qhandle_t   torsoModel;
  qhandle_t   torsoSkin;

  qhandle_t   headModel;
  qhandle_t   headSkin;

  qhandle_t   nonSegModel;                //non-segmented model system
  qhandle_t   nonSegSkin;                 //non-segmented model system

  qhandle_t   modelIcon;

  animation_t animations[ MAX_PLAYER_TOTALANIMATIONS ];

  sfxHandle_t sounds[ MAX_CUSTOM_SOUNDS ];

  sfxHandle_t customFootsteps[ 4 ];
  sfxHandle_t customMetalFootsteps[ 4 ];

  char        voice[ MAX_VOICE_NAME_LEN ];
  int         voiceTime;
};


struct weaponInfoMode_t
{
  float       flashDlight;
  vec3_t      flashDlightColor;
  sfxHandle_t flashSound[ 4 ];  // fast firing weapons randomly choose
  bool    continuousFlash;

  qhandle_t   missileModel;
  sfxHandle_t missileSound;
  float       missileDlight;
  vec3_t      missileDlightColor;
  int         missileRenderfx;
  bool    usesSpriteMissle;
  qhandle_t   missileSprite;
  int         missileSpriteSize;
  float       missileSpriteCharge;
  qhandle_t   missileParticleSystem;
  qhandle_t   missileTrailSystem;
  bool    missileRotates;
  bool    missileAnimates;
  int         missileAnimStartFrame;
  int         missileAnimNumFrames;
  int         missileAnimFrameRate;
  int         missileAnimLooping;

  sfxHandle_t firingSound;

  qhandle_t   muzzleParticleSystem;

  bool    alwaysImpact;
  qhandle_t   impactParticleSystem;
  qhandle_t   impactMark;
  qhandle_t   impactMarkSize;
  sfxHandle_t impactSound[ 4 ]; //random impact sound
  sfxHandle_t impactFleshSound[ 4 ]; //random impact sound
};

// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
struct weaponInfo_t
{
  bool          registered;
  const char        *humanName;

  qhandle_t         handsModel;       // the hands don't actually draw, they just position the weapon
  qhandle_t         weaponModel;
  qhandle_t         barrelModel;
  qhandle_t         flashModel;

  qhandle_t         weaponModel3rdPerson;
  qhandle_t         barrelModel3rdPerson;
  qhandle_t         flashModel3rdPerson;

  animation_t       animations[ MAX_WEAPON_ANIMATIONS ];
  bool          noDrift;

  vec3_t            weaponMidpoint;   // so it will rotate centered instead of by tag

  qhandle_t         weaponIcon;
  qhandle_t         ammoIcon;

  qhandle_t         crossHair;
  int               crossHairSize;

  sfxHandle_t       readySound;

  bool          disableIn3rdPerson;

  weaponInfoMode_t  wim[ WPM_NUM_WEAPONMODES ];
};

struct upgradeInfo_t
{
  bool    registered;
  const char  *humanName;

  qhandle_t   upgradeIcon;
};

struct sound_t
{
  bool    looped;
  bool    enabled;

  sfxHandle_t sound;
};

struct buildableInfo_t
{
  qhandle_t   models[ MAX_BUILDABLE_MODELS ];
  animation_t animations[ MAX_BUILDABLE_ANIMATIONS ];

  //same number of sounds as animations
  sound_t     sounds[ MAX_BUILDABLE_ANIMATIONS ];
};

#define MAX_REWARDSTACK   10
#define MAX_SOUNDBUFFER   20

//======================================================================

struct entityPos_t
{
  vec3_t    alienBuildablePos[ MAX_GENTITIES ];
  int       alienBuildableTimes[ MAX_GENTITIES ];
  int       numAlienBuildables;

  vec3_t    humanBuildablePos[ MAX_GENTITIES ];
  int       numHumanBuildables;

  vec3_t    alienClientPos[ MAX_CLIENTS ];
  int       numAlienClients;

  vec3_t    humanClientPos[ MAX_CLIENTS ];
  int       numHumanClients;

  int       lastUpdateTime;
  vec3_t    origin;
  vec3_t    vangles;
};

struct consoleLine_t
{
  int time;
  int length;
};

#define MAX_CONSOLE_TEXT  8192
#define MAX_CONSOLE_LINES 32

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS  16

#define NUM_SAVED_STATES ( CMD_BACKUP + 2 )

// After this many msec the crosshair name fades out completely
#define CROSSHAIR_CLIENT_TIMEOUT 1000

struct cg_t
{
  int           clientFrame;                        // incremented each frame

  int           clientNum;

  bool      demoPlayback;
  bool      levelShot;                          // taking a level menu screenshot
  int           deferredPlayerLoading;
  bool      loading;                            // don't defer players at initial startup
  bool      intermissionStarted;                // don't play voice rewards, because game will end shortly

  // there are only one or two snapshot_t that are relevent at a time
  int           latestSnapshotNum;                  // the number of snapshots the client system has received
  int           latestSnapshotTime;                 // the time from latestSnapshotNum, so we don't need to read the snapshot yet

  snapshot_t    *snap;                              // cg.snap->serverTime <= cg.time
  snapshot_t    *nextSnap;                          // cg.nextSnap->serverTime > cg.time, or NULL
  snapshot_t    activeSnapshots[ 2 ];

  float         frameInterpolation;                 // (float)( cg.time - cg.frame->serverTime ) /
                                                    // (cg.nextFrame->serverTime - cg.frame->serverTime)

  bool      thisFrameTeleport;
  bool      nextFrameTeleport;

  int           frametime;                          // cg.time - cg.oldTime

  int           time;                               // this is the time value that the client
                                                    // is rendering at.
  int           oldTime;                            // time at last frame, used for missile trails and prediction checking

  int           physicsTime;                        // either cg.snap->time or cg.nextSnap->time

  int           timelimitWarnings;                  // 5 min, 1 min, overtime
  int           fraglimitWarnings;

  bool      mapRestart;                         // set on a map restart to set back the weapon

  bool      renderingThirdPerson;               // during deaths, chasecams, etc

  // prediction state
  bool      hyperspace;                         // true if prediction has hit a trigger_teleport
  playerState_t predictedPlayerState;
  pmoveExt_t    pmext;
  centity_t     predictedPlayerEntity;
  bool      validPPS;                           // clear until the first call to CG_PredictPlayerState
  int           predictedErrorTime;
  vec3_t        predictedError;

  int           eventSequence;
  int           predictableEvents[MAX_PREDICTED_EVENTS];

  float         stepChange;                         // for stair up smoothing
  int           stepTime;

  float         duckChange;                         // for duck viewheight smoothing
  int           duckTime;

  float         landChange;                         // for landing hard
  int           landTime;

  // input state sent to server
  int           weaponSelect;

  // auto rotating items
  vec3_t        autoAngles;
  vec3_t        autoAxis[ 3 ];
  vec3_t        autoAnglesFast;
  vec3_t        autoAxisFast[ 3 ];

  // view rendering
  refdef_t      refdef;
  vec3_t        refdefViewAngles;                   // will be converted to refdef.viewaxis

  // zoom key
  bool      zoomed;
  int           zoomTime;
  float         zoomSensitivity;

  // information screen text during loading
  char          infoScreenText[ MAX_STRING_CHARS ];

  // scoreboard
  int           scoresRequestTime;
  int           numScores;
  int           selectedScore;
  int           teamScores[ 2 ];
  score_t       scores[MAX_CLIENTS];
  bool      showScores;
  bool      scoreBoardShowing;
  int           scoreFadeTime;
  char          killerName[ MAX_NAME_LENGTH ];
  char          spectatorList[ MAX_STRING_CHARS ];  // list of names
  int           spectatorTime;                      // next time to offset
  float         spectatorOffset;                    // current offset from start

  // centerprinting
  int           centerPrintTime;
  int           centerPrintCharWidth;
  int           centerPrintY;
  char          centerPrint[ MAX_STRING_CHARS ];
  int           centerPrintLines;

  // low ammo warning state
  int           lowAmmoWarning;   // 1 = low, 2 = empty

  // kill timers for carnage reward
  int           lastKillTime;

  // crosshair client ID
  int           crosshairBuildable;
  int           crosshairClientNum;
  int           crosshairClientTime;

  // powerup active flashing
  int           powerupActive;
  int           powerupTime;

  // attacking player
  int           attackerTime;

  // reward medals
  int           rewardStack;
  int           rewardTime;
  int           rewardCount[ MAX_REWARDSTACK ];
  qhandle_t     rewardShader[ MAX_REWARDSTACK ];
  qhandle_t     rewardSound[ MAX_REWARDSTACK ];

  // sound buffer mainly for announcer sounds
  int           soundBufferIn;
  int           soundBufferOut;
  int           soundTime;
  qhandle_t     soundBuffer[ MAX_SOUNDBUFFER ];

  // for voice chat buffer
  int           voiceChatTime;
  int           voiceChatBufferIn;
  int           voiceChatBufferOut;

  // warmup countdown
  int           warmupTime;

  //==========================

  int           itemPickup;
  int           itemPickupTime;
  int           itemPickupBlendTime;                // the pulse around the crosshair is timed seperately

  int           weaponSelectTime;
  int           weaponAnimation;
  int           weaponAnimationTime;

  // blend blobs
  float         damageTime;
  float         damageX, damageY, damageValue;

  // status bar head
  float         headYaw;
  float         headEndPitch;
  float         headEndYaw;
  int           headEndTime;
  float         headStartPitch;
  float         headStartYaw;
  int           headStartTime;

  // view movement
  float         v_dmg_time;
  float         v_dmg_pitch;
  float         v_dmg_roll;

  bool      chaseFollow;

  // temp working variables for player view
  float         bobfracsin;
  int           bobcycle;
  float         xyspeed;
  int           nextOrbitTime;

  // development tool
  refEntity_t   testModelEntity;
  refEntity_t   testModelBarrelEntity;
  char          testModelName[MAX_QPATH];
  char          testModelBarrelName[MAX_QPATH];
  bool      testGun;

  int           spawnTime;                          // fovwarp
  int           weapon1Time;                        // time when BUTTON_ATTACK went t->f f->t
  int           weapon2Time;                        // time when BUTTON_ATTACK2 went t->f f->t
  int           weapon3Time;                        // time when BUTTON_USE_HOLDABLE went t->f f->t
  bool      weapon1Firing;
  bool      weapon2Firing;
  bool      weapon3Firing;

  int           poisonedTime;

  vec3_t        lastNormal;                         // view smoothage
  vec3_t        lastVangles;                        // view smoothage
  smooth_t      sList[ MAXSMOOTHS ];                // WW smoothing

  int           forwardMoveTime;                    // for struggling
  int           rightMoveTime;
  int           upMoveTime;

  float         charModelFraction;                  // loading percentages
  float         mediaFraction;
  float         buildablesFraction;

  int           lastBuildAttempt;
  int           lastEvolveAttempt;

  char          consoleText[ MAX_CONSOLE_TEXT ];
  consoleLine_t consoleLines[ MAX_CONSOLE_LINES ];
  int           numConsoleLines;

  particleSystem_t  *poisonCloudPS;
  particleSystem_t  *poisonCloudedPS;

  float         painBlendValue;
  float         painBlendTarget;
  float         healBlendValue;
  int           lastHealth;
  bool      wasDeadLastFrame;

  int           lastPredictedCommand;
  int           lastServerTime;
  playerState_t savedPmoveStates[ NUM_SAVED_STATES ];
  int           stateHead, stateTail;
  int           ping;
  
  float         chargeMeterAlpha;
  float         chargeMeterValue;
  qhandle_t     lastHealthCross;
  float         healthCrossFade;
  int           nearUsableBuildable;
  
  int           nextWeaponClickTime;
  // binary shaders - by /dev/humancontroller
  int           numBinaryShadersUsed;
  cgBinaryShaderSetting_t binaryShaderSettings[ NUM_BINARY_SHADERS ];
};

struct cgMedia_t
{
  qhandle_t   charsetShader;
  qhandle_t   whiteShader;
  qhandle_t   outlineShader;

  qhandle_t   level2ZapTS;

  qhandle_t   balloonShader;
  qhandle_t   connectionShader;

  qhandle_t   viewBloodShader;
  qhandle_t   tracerShader;
  qhandle_t   crosshairShader[ WP_NUM_WEAPONS ];
  qhandle_t   backTileShader;

  qhandle_t   creepShader;

  qhandle_t   scannerShader;
  qhandle_t   scannerBlipShader;
  qhandle_t   scannerLineShader;

  qhandle_t   teamOverlayShader;

  qhandle_t   numberShaders[ 11 ];

  qhandle_t   shadowMarkShader;
  qhandle_t   wakeMarkShader;

  // buildable shaders
  qhandle_t   greenBuildShader;
  qhandle_t   redBuildShader;
  qhandle_t   humanSpawningShader;

  // binary shaders + range markers
  qhandle_t   sphereModel;
  qhandle_t   sphericalCone64Model;
  qhandle_t   sphericalCone240Model;

  qhandle_t   plainColorShader;
  qhandle_t   binaryAlpha1Shader;
  cgMediaBinaryShader_t binaryShaders[ NUM_BINARY_SHADERS ];

  // disconnect
  qhandle_t   disconnectPS;
  qhandle_t   disconnectSound;

  // sounds
  sfxHandle_t tracerSound;
  sfxHandle_t weaponEmptyClick;
  sfxHandle_t selectSound;
  sfxHandle_t footsteps[ FOOTSTEP_TOTAL ][ 4 ];
  sfxHandle_t talkSound;
  sfxHandle_t alienTalkSound;
  sfxHandle_t humanTalkSound;
  sfxHandle_t landSound;
  sfxHandle_t fallSound;
  sfxHandle_t turretSpinupSound;

  sfxHandle_t hardBounceSound1;
  sfxHandle_t hardBounceSound2;

  sfxHandle_t voteNow;
  sfxHandle_t votePassed;
  sfxHandle_t voteFailed;

  sfxHandle_t watrInSound;
  sfxHandle_t watrOutSound;
  sfxHandle_t watrUnSound;

  sfxHandle_t jetpackDescendSound;
  sfxHandle_t jetpackIdleSound;
  sfxHandle_t jetpackAscendSound;

  qhandle_t   jetPackDescendPS;
  qhandle_t   jetPackHoverPS;
  qhandle_t   jetPackAscendPS;

  sfxHandle_t medkitUseSound;

  sfxHandle_t alienStageTransition;
  sfxHandle_t humanStageTransition;

  sfxHandle_t alienOvermindAttack;
  sfxHandle_t alienOvermindDying;
  sfxHandle_t alienOvermindSpawns;

  sfxHandle_t alienBuildableExplosion;
  sfxHandle_t alienBuildableDamage;
  sfxHandle_t alienBuildablePrebuild;
  sfxHandle_t humanBuildableExplosion;
  sfxHandle_t humanBuildablePrebuild;
  sfxHandle_t humanBuildableDamage[ 4 ];

  sfxHandle_t alienL1Grab;
  sfxHandle_t alienL4ChargePrepare;
  sfxHandle_t alienL4ChargeStart;

  qhandle_t   cursor;
  qhandle_t   selectCursor;
  qhandle_t   sizeCursor;

  //light armour
  qhandle_t larmourHeadSkin;
  qhandle_t larmourLegsSkin;
  qhandle_t larmourTorsoSkin;

  qhandle_t jetpackModel;
  qhandle_t jetpackFlashModel;
  qhandle_t battpackModel;

  sfxHandle_t repeaterUseSound;

  sfxHandle_t buildableRepairSound;
  sfxHandle_t buildableRepairedSound;

  qhandle_t   poisonCloudPS;
  qhandle_t   poisonCloudedPS;
  qhandle_t   alienEvolvePS;
  qhandle_t   alienAcidTubePS;

  sfxHandle_t alienEvolveSound;

  qhandle_t   humanBuildableDamagedPS;
  qhandle_t   humanBuildableDestroyedPS;
  qhandle_t   alienBuildableDamagedPS;
  qhandle_t   alienBuildableDestroyedPS;

  qhandle_t   alienBleedPS;
  qhandle_t   humanBleedPS;
  qhandle_t   alienBuildableBleedPS;
  qhandle_t   humanBuildableBleedPS;


  qhandle_t   teslaZapTS;

  sfxHandle_t lCannonWarningSound;
  sfxHandle_t lCannonWarningSound2;

  qhandle_t   buildWeaponTimerPie[ 8 ];
  qhandle_t   upgradeClassIconShader;
  qhandle_t   healthCross;
  qhandle_t   healthCross2X;
  qhandle_t   healthCross3X;
  qhandle_t   healthCrossMedkit;
  qhandle_t   healthCrossPoisoned;
};

struct buildStat_t
{
  qhandle_t     frameShader;
  qhandle_t     overlayShader;
  qhandle_t     noPowerShader;
  qhandle_t     markedShader;
  vec4_t        healthSevereColor;
  vec4_t        healthHighColor;
  vec4_t        healthElevatedColor;
  vec4_t        healthGuardedColor;
  vec4_t        healthLowColor;
  int           frameHeight;
  int           frameWidth;
  int           healthPadding;
  int           overlayHeight;
  int           overlayWidth;
  float         verticalMargin;
  float         horizontalMargin;
  vec4_t        foreColor;
  vec4_t        backColor;
  bool      loaded;
};


// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
struct cgs_t
{
  gameState_t   gameState;              // gamestate from server
  glconfig_t    glconfig;               // rendering configuration
  float         screenXScale;           // derived from glconfig
  float         screenYScale;
  float         screenXBias;

  int           serverCommandSequence;  // reliable command stream counter
  int           processedSnapshotNum;   // the number of snapshots cgame has requested

  bool      localServer;            // detected on startup by checking sv_running

  // parsed from serverinfo
  int           timelimit;
  int           maxclients;
  char          mapname[ MAX_QPATH ];
  bool      markDeconstruct;        // Whether or not buildables are marked

  int           voteTime[ NUM_TEAMS ];
  int           voteYes[ NUM_TEAMS ];
  int           voteNo[ NUM_TEAMS ];
  char          voteCaller[ NUM_TEAMS ][ MAX_NAME_LENGTH ];
  bool      voteModified[ NUM_TEAMS ];// beep whenever changed
  char          voteString[ NUM_TEAMS ][ MAX_STRING_TOKENS ];

  int           levelStartTime;

  int           scores1, scores2;   // from configstrings

  bool      newHud;

  int           alienStage;
  int           humanStage;
  int           alienCredits;
  int           humanCredits;
  int           alienNextStageThreshold;
  int           humanNextStageThreshold;

  //
  // locally derived information from gamestate
  //
  qhandle_t     gameModels[ MAX_MODELS ];
  qhandle_t     gameShaders[ MAX_GAME_SHADERS ];
  qhandle_t     gameParticleSystems[ MAX_GAME_PARTICLE_SYSTEMS ];
  sfxHandle_t   gameSounds[ MAX_SOUNDS ];

  int           numInlineModels;
  qhandle_t     inlineDrawModel[ MAX_MODELS ];
  vec3_t        inlineModelMidpoints[ MAX_MODELS ];

  clientInfo_t  clientinfo[ MAX_CLIENTS ];

  int teaminfoReceievedTime;

  // corpse info
  clientInfo_t  corpseinfo[ MAX_CLIENTS ];

  int           cursorX;
  int           cursorY;
  int           eventHandling;
  bool      mouseCaptured;
  bool      sizingHud;
  menuDef_t     *capturedMenu;
  qhandle_t     activeCursor;

  buildStat_t   alienBuildStat;
  buildStat_t   humanBuildStat;

  // media
  cgMedia_t           media;

  voice_t       *voices;
  clientList_t  ignoreList;

  // Kill Message
  char          killMsgKillers[ TEAMCHAT_HEIGHT ][ 33*3+1 ];
  char          killMsgVictims[ TEAMCHAT_HEIGHT ][ 33*3+1 ];
  int           killMsgWeapons[ TEAMCHAT_HEIGHT ];
  int           killMsgMsgTimes[ TEAMCHAT_HEIGHT ];
  int           killMsgPos;
  int           killMsgLastPos;
};

struct consoleCommand_t
{
  const char *cmd;
  void ( *function )( void );
};

struct consoleCommandCompletions_t
{
  const char *cmd;
  void (*function)( int argNum );
};

//==============================================================================

extern  cgs_t     cgs;
extern  cg_t      cg;
extern  centity_t cg_entities[ MAX_GENTITIES ];
extern  displayContextDef_t  cgDC;
extern  refexport_t* re;

extern  weaponInfo_t    cg_weapons[ 32 ];
extern  upgradeInfo_t   cg_upgrades[ 32 ];

extern  buildableInfo_t cg_buildables[ BA_NUM_BUILDABLES ];

extern  markPoly_t      cg_markPolys[ MAX_MARK_POLYS ];

extern  vmCvar_t    cg_teslaTrailTime;
extern  vmCvar_t    cg_centertime;
extern  vmCvar_t    cg_runpitch;
extern  vmCvar_t    cg_runroll;
extern  vmCvar_t    cg_swingSpeed;
extern  vmCvar_t    cg_shadows;
extern  vmCvar_t    cg_drawTimer;
extern  vmCvar_t    cg_drawClock;
extern  vmCvar_t    cg_drawFPS;
extern  vmCvar_t    cg_drawDemoState;
extern  vmCvar_t    cg_drawSnapshot;
extern  vmCvar_t    cg_drawChargeBar;
extern  vmCvar_t    cg_drawCrosshair;
extern  vmCvar_t    cg_drawCrosshairNames;
extern  vmCvar_t    cg_crosshairSize;
extern  vmCvar_t    cg_drawTeamOverlay;
extern  vmCvar_t    cg_teamOverlaySortMode;
extern  vmCvar_t    cg_teamOverlayMaxPlayers;
extern  vmCvar_t    cg_teamOverlayUserinfo;
extern  vmCvar_t    cg_draw2D;
extern  vmCvar_t    cg_animSpeed;
extern  vmCvar_t    cg_debugAnim;
extern  vmCvar_t    cg_debugPosition;
extern  vmCvar_t    cg_debugEvents;
extern  vmCvar_t    cg_errorDecay;
extern  vmCvar_t    cg_nopredict;
extern  vmCvar_t    cg_debugMove;
extern  vmCvar_t    cg_noPlayerAnims;
extern  vmCvar_t    cg_showmiss;
extern  vmCvar_t    cg_footsteps;
extern  vmCvar_t    cg_addMarks;
extern  vmCvar_t    cg_viewsize;
extern  vmCvar_t    cg_drawGun;
extern  vmCvar_t    cg_gun_frame;
extern  vmCvar_t    cg_gun_x;
extern  vmCvar_t    cg_gun_y;
extern  vmCvar_t    cg_gun_z;
extern  vmCvar_t    cg_tracerChance;
extern  vmCvar_t    cg_tracerWidth;
extern  vmCvar_t    cg_tracerLength;
extern  vmCvar_t    cg_thirdPerson;
extern  vmCvar_t    cg_thirdPersonAngle;
extern  vmCvar_t    cg_thirdPersonShoulderViewMode;
extern  vmCvar_t    cg_staticDeathCam;
extern  vmCvar_t    cg_thirdPersonPitchFollow;
extern  vmCvar_t    cg_thirdPersonRange;
extern  vmCvar_t    cg_stereoSeparation;
extern  vmCvar_t    cg_lagometer;
extern  vmCvar_t    cg_drawSpeed;
extern  vmCvar_t    cg_maxSpeedTimeWindow;
extern  vmCvar_t    cg_synchronousClients;
extern  vmCvar_t    cg_stats;
extern  vmCvar_t    cg_paused;
extern  vmCvar_t    cg_blood;
extern  vmCvar_t    cg_teamOverlayUserinfo;
extern  vmCvar_t    cg_teamChatsOnly;
extern  vmCvar_t    cg_noVoiceChats;
extern  vmCvar_t    cg_noVoiceText;
extern  vmCvar_t    cg_hudFiles;
extern  vmCvar_t    cg_smoothClients;
extern  vmCvar_t    pmove_fixed;
extern  vmCvar_t    pmove_msec;
extern  vmCvar_t    cg_cameraMode;
extern  vmCvar_t    cg_timescaleFadeEnd;
extern  vmCvar_t    cg_timescaleFadeSpeed;
extern  vmCvar_t    cg_timescale;
extern  vmCvar_t    cg_noTaunt;
extern  vmCvar_t    cg_drawSurfNormal;
extern  vmCvar_t    cg_drawBBOX;
extern  vmCvar_t    cg_wwSmoothTime;
extern  vmCvar_t    cg_disableBlueprintErrors;
extern  vmCvar_t    cg_depthSortParticles;
extern  vmCvar_t    cg_bounceParticles;
extern  vmCvar_t    cg_consoleLatency;
extern  vmCvar_t    cg_lightFlare;
extern  vmCvar_t    cg_debugParticles;
extern  vmCvar_t    cg_debugTrails;
extern  vmCvar_t    cg_debugPVS;
extern  vmCvar_t    cg_disableWarningDialogs;
extern  vmCvar_t    cg_disableUpgradeDialogs;
extern  vmCvar_t    cg_disableBuildDialogs;
extern  vmCvar_t    cg_disableCommandDialogs;
extern  vmCvar_t    cg_disableScannerPlane;
extern  vmCvar_t    cg_tutorial;

extern  vmCvar_t    cg_rangeMarkerDrawSurface;
extern  vmCvar_t    cg_rangeMarkerDrawIntersection;
extern  vmCvar_t    cg_rangeMarkerDrawFrontline;
extern  vmCvar_t    cg_rangeMarkerSurfaceOpacity;
extern  vmCvar_t    cg_rangeMarkerLineOpacity;
extern  vmCvar_t    cg_rangeMarkerLineThickness;
extern  vmCvar_t    cg_rangeMarkerForBlueprint;
extern  vmCvar_t    cg_rangeMarkerBuildableTypes;
extern  vmCvar_t    cg_binaryShaderScreenScale;

extern  vmCvar_t    cg_painBlendUpRate;
extern  vmCvar_t    cg_painBlendDownRate;
extern  vmCvar_t    cg_painBlendMax;
extern  vmCvar_t    cg_painBlendScale;
extern  vmCvar_t    cg_painBlendZoom;

extern  vmCvar_t    cg_stickySpec;
extern  vmCvar_t    cg_sprintToggle;
extern  vmCvar_t    cg_unlagged;

extern  vmCvar_t    cg_debugVoices;

extern  vmCvar_t    ui_currentClass;
extern  vmCvar_t    ui_carriage;
extern  vmCvar_t    ui_stages;
extern  vmCvar_t    ui_dialog;
extern  vmCvar_t    ui_voteActive;
extern  vmCvar_t    ui_alienTeamVoteActive;
extern  vmCvar_t    ui_humanTeamVoteActive;

extern  vmCvar_t    cg_debugRandom;

extern  vmCvar_t    cg_optimizePrediction;
extern  vmCvar_t    cg_projectileNudge;

extern  vmCvar_t    cg_voice;

extern  vmCvar_t    cg_emoticons;

extern  vmCvar_t    cg_chatTeamPrefix;

extern  vmCvar_t    cg_killMsg;
extern  vmCvar_t    cg_killMsgTime;
extern  vmCvar_t    cg_killMsgHeight;

extern  vmCvar_t    cg_killMsgTime;
extern  vmCvar_t    cg_killMsgHeight;

extern  vmCvar_t    thz_radar;
extern  vmCvar_t    thz_radarrange;

//
// cg_main.c
//
const char  *CG_ConfigString( int index );
const char  *CG_Argv( int arg );

void QDECL  CG_Printf( const char *msg, ... ) __attribute__ ((format (printf, 1, 2)));
void QDECL  CG_Error( const char *msg, ... ) __attribute__ ((noreturn, format (printf, 1, 2)));

void        CG_StartMusic( void );
int         CG_PlayerCount( void );

void        CG_UpdateCvars( void );

int         CG_CrosshairPlayer( void );
int         CG_LastAttacker( void );
void        CG_LoadMenus( const char *menuFile );
void        CG_KeyEvent( int key, bool down );
void        CG_MouseEvent( int x, int y );
void        CG_EventHandling( int type );
void        CG_SetScoreSelection( void *menu );
bool    CG_ClientIsReady( int clientNum );
void        CG_BuildSpectatorString( void );

int    CG_FileExists( const char *filename );
void        CG_RemoveNotifyLine( void );
void        CG_AddNotifyText( void );
bool    CG_GetRangeMarkerPreferences( bool *drawSurface, bool *drawIntersection,
                                          bool *drawFrontline, float *surfaceOpacity,
                                          float *lineOpacity, float *lineThickness );
void        CG_UpdateBuildableRangeMarkerMask( void );


//
// cg_view.c
//
void        CG_addSmoothOp( vec3_t rotAxis, float rotAngle, float timeMod );
void        CG_TestModel_f( void );
void        CG_TestGun_f( void );
void        CG_TestModelNextFrame_f( void );
void        CG_TestModelPrevFrame_f( void );
void        CG_TestModelNextSkin_f( void );
void        CG_TestModelPrevSkin_f( void );
void        CG_AddBufferedSound( sfxHandle_t sfx );
void        CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, bool demoPlayback );
void        CG_OffsetFirstPersonView( void );
void        CG_OffsetThirdPersonView( void );
void        CG_OffsetShoulderView( void );


//
// cg_drawtools.c
//
void        CG_DrawPlane( vec3_t origin, vec3_t down, vec3_t right, qhandle_t shader );
void        CG_AdjustFrom640( float *x, float *y, float *w, float *h );
void        CG_FillRect( float x, float y, float width, float height, const float *color );
void        CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void        CG_DrawFadePic( float x, float y, float width, float height, vec4_t fcolor,
                            vec4_t tcolor, float amount, qhandle_t hShader );
void        CG_SetClipRegion( float x, float y, float w, float h );
void        CG_ClearClipRegion( void );

int         CG_DrawStrlen( const char *str );

float       *CG_FadeColor( int startMsec, int totalMsec );
void        CG_TileClear( void );
void        CG_ColorForHealth( vec4_t hcolor );
void        CG_DrawRect( float x, float y, float width, float height, float size, const float *color );
void        CG_DrawSides(float x, float y, float w, float h, float size);
void        CG_DrawTopBottom(float x, float y, float w, float h, float size);
bool    CG_WorldToScreen( vec3_t point, float *x, float *y );
char        *CG_KeyBinding( const char *bind );
char        CG_GetColorCharForHealth( int clientnum );
void        CG_DrawSphere( const vec3_t center, float radius, int customShader, const float *shaderRGBA );
void        CG_DrawSphericalCone( const vec3_t tip, const vec3_t rotation, float radius,
                                  bool a240, int customShader, const float *shaderRGBA );
void        CG_DrawRangeMarker( rangeMarkerType_t rmType, const vec3_t origin, const float *angles, float range,
                                bool drawSurface, bool drawIntersection, bool drawFrontline,
                                const vec3_t rgb, float surfaceOpacity, float lineOpacity, float lineThickness );

//
// cg_draw.c
//

void        CG_AddLagometerFrameInfo( void );
void        CG_AddLagometerSnapshotInfo( snapshot_t *snap );
void        CG_AddSpeed( void );
void        CG_CenterPrint( const char *str, int y, int charWidth );
void        CG_DrawActive( stereoFrame_t stereoView );
void        CG_OwnerDraw( float x, float y, float w, float h, float text_x,
                          float text_y, int ownerDraw, int ownerDrawFlags,
                          int align, int textalign, int textvalign,
                          float borderSize, float scale, vec4_t foreColor,
                          vec4_t backColor, qhandle_t shader, int textStyle );
float       CG_GetValue(int ownerDraw);
void        CG_RunMenuScript(char **args);
void        CG_SetPrintString( int type, const char *p );
void        CG_GetTeamColor( vec4_t *color );
const char  *CG_GetKillerText( void );
void        CG_Text_PaintChar( float x, float y, float width, float height, float scale,
                               float s, float t, float s2, float t2, qhandle_t hShader );
void        CG_DrawLoadingScreen( void );
void        CG_UpdateMediaFraction( float newFract );
void        CG_ResetPainBlend( void );
void        CG_DrawField( float x, float y, int width, float cw, float ch, int value );

//
// cg_players.c
//
void        CG_Player( centity_t *cent );
void        CG_Corpse( centity_t *cent );
void        CG_ResetPlayerEntity( centity_t *cent );
void        CG_NewClientInfo( int clientNum );
void        CG_PrecacheClientInfo( class_t klass, char *model, char *skin );
sfxHandle_t CG_CustomSound( int clientNum, const char *soundName );
void        CG_PlayerDisconnect( vec3_t org );
void        CG_Bleed( vec3_t origin, vec3_t normal, int entityNum );
centity_t   *CG_GetPlayerLocation( void );

//
// cg_buildable.c
//
void        CG_GhostBuildable( buildable_t buildable );
void        CG_Buildable( centity_t *cent );
void        CG_BuildableStatusParse( const char *filename, buildStat_t *bs );
void        CG_DrawBuildableStatus( void );
void        CG_InitBuildables( void );
void        CG_HumanBuildableExplosion( vec3_t origin, vec3_t dir );
void        CG_AlienBuildableExplosion( vec3_t origin, vec3_t dir );
bool    CG_GetBuildableRangeMarkerProperties( buildable_t bType, rangeMarkerType_t *rmType, float *range, vec3_t rgb );

//
// cg_animation.c
//
void        CG_RunLerpFrame( lerpFrame_t *lf, float scale );

//
// cg_animmapobj.c
//
void        CG_AnimMapObj( centity_t *cent );
void        CG_ModelDoor( centity_t *cent );

//
// cg_predict.c
//

#define MAGIC_TRACE_HACK -2

void        CG_BuildSolidList( void );
int         CG_PointContents( const vec3_t point, int passEntityNum );
void        CG_Trace( trace_t *result, const vec3_t start, vec3_t mins, vec3_t maxs,
                const vec3_t end, int skipNumber, int mask );
void        CG_CapTrace( trace_t *result, const vec3_t start, vec3_t mins, vec3_t maxs,
                const vec3_t end, int skipNumber, int mask );
void        CG_BiSphereTrace( trace_t *result, const vec3_t start, const vec3_t end,
                const float startRadius, const float endRadius, int skipNumber, int mask );
void        CG_PredictPlayerState( void );


//
// cg_events.c
//
void        CG_CheckEvents( centity_t *cent );
void        CG_EntityEvent( centity_t *cent, vec3_t position );
void        CG_PainEvent( centity_t *cent, int health );

void          CG_AddToKillMsg( const char* killername, const char* victimname, int icon );

//
// cg_ents.c
//
void        CG_DrawBoundingBox( vec3_t origin, vec3_t mins, vec3_t maxs );
void        CG_SetEntitySoundPosition( centity_t *cent );
void        CG_AddPacketEntities( void );
void        CG_Beam( centity_t *cent );
void        CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out );

void        CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
                                    qhandle_t parentModel, const char *tagName );
void        CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
                                           qhandle_t parentModel, const char *tagName );
void        CG_RangeMarker( centity_t *cent );


//
// cg_weapons.c
//
void        CG_NextWeapon_f( void );
void        CG_PrevWeapon_f( void );
void        CG_Weapon_f( void );

void        CG_InitUpgrades( void );
void        CG_RegisterUpgrade( int upgradeNum );
void        CG_InitWeapons( void );
void        CG_RegisterWeapon( int weaponNum );

void        CG_FireWeapon( centity_t *cent, weaponMode_t weaponMode );
void        CG_MissileHitWall( weapon_t weapon, weaponMode_t weaponMode, int clientNum,
                               vec3_t origin, vec3_t dir, impactSound_t soundType, int charge );
void        CG_MissileHitEntity( weapon_t weaponNum, weaponMode_t weaponMode,
                                 vec3_t origin, vec3_t dir, int entityNum, int charge );
void        CG_Bullet( vec3_t origin, int sourceEntityNum, vec3_t normal, bool flesh, int fleshEntityNum );
void        CG_ShotgunFire( entityState_t *es );

void        CG_AddViewWeapon (playerState_t *ps);
void        CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent );
void        CG_DrawItemSelect( Rectangle *rect, vec4_t color );
void        CG_DrawItemSelectText( Rectangle *rect, float scale, int textStyle );


//
// cg_scanner.c
//
void        CG_UpdateEntityPositions( void );
void        CG_Scanner( Rectangle *rect, qhandle_t shader, vec4_t color );
void        CG_AlienSense( Rectangle *rect );
void        THZ_DrawScanner( Rectangle *rect );

//
// cg_marks.c
//
void        CG_InitMarkPolys( void );
void        CG_AddMarks( void );
void        CG_ImpactMark( qhandle_t markShader,
                           const vec3_t origin, const vec3_t dir,
                           float orientation,
                           float r, float g, float b, float a,
                           bool alphaFade,
                           float radius, bool temporary );

//
// cg_snapshot.c
//
void          CG_ProcessSnapshots( void );

//
// cg_consolecmds.c
//
bool CG_Console_CompleteArgument( int argNum );
bool      CG_ConsoleCommand( void );
void          CG_InitConsoleCommands( void );
bool      CG_RequestScores( void );

//
// cg_servercmds.c
//
void          CG_ExecuteNewServerCommands( int latestSequence );
void          CG_ParseServerinfo( void );
void          CG_SetConfigValues( void );
void          CG_ShaderStateChanged(void);
void          CG_UnregisterCommands( void );
void          CG_CenterPrint_f( void );

//
// cg_playerstate.c
//
void          CG_Respawn( void );
void          CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops );
void          CG_CheckChangedPredictableEvents( playerState_t *ps );

//
// cg_attachment.c
//
bool    CG_AttachmentPoint( attachment_t *a, vec3_t v );
bool    CG_AttachmentDir( attachment_t *a, vec3_t v );
bool    CG_AttachmentAxis( attachment_t *a, vec3_t axis[ 3 ] );
bool    CG_AttachmentVelocity( attachment_t *a, vec3_t v );
int         CG_AttachmentCentNum( attachment_t *a );

bool    CG_Attached( attachment_t *a );

void        CG_AttachToPoint( attachment_t *a );
void        CG_AttachToCent( attachment_t *a );
void        CG_AttachToTag( attachment_t *a );
void        CG_AttachToParticle( attachment_t *a );
void        CG_SetAttachmentPoint( attachment_t *a, vec3_t v );
void        CG_SetAttachmentCent( attachment_t *a, centity_t *cent );
void        CG_SetAttachmentTag( attachment_t *a, refEntity_t parent,
                qhandle_t model, const char *tagName );
void        CG_SetAttachmentParticle( attachment_t *a, particle_t *p );

void        CG_SetAttachmentOffset( attachment_t *a, vec3_t v );

//
// cg_particles.c
//
void                CG_LoadParticleSystems( void );
qhandle_t           CG_RegisterParticleSystem( const char *name );

particleSystem_t    *CG_SpawnNewParticleSystem( qhandle_t psHandle );
void                CG_DestroyParticleSystem( particleSystem_t **ps );

bool            CG_IsParticleSystemInfinite( particleSystem_t *ps );
bool            CG_IsParticleSystemValid( particleSystem_t **ps );

void                CG_SetParticleSystemNormal( particleSystem_t *ps, vec3_t normal );
void                CG_SetParticleSystemLastNormal( particleSystem_t *ps, const float *normal );

void                CG_AddParticles( void );

void                CG_ParticleSystemEntity( centity_t *cent );

void                CG_TestPS_f( void );
void                CG_DestroyTestPS_f( void );

//
// cg_trails.c
//
void                CG_LoadTrailSystems( void );
qhandle_t           CG_RegisterTrailSystem( const char *name );

trailSystem_t       *CG_SpawnNewTrailSystem( qhandle_t psHandle );
void                CG_DestroyTrailSystem( trailSystem_t **ts );

bool            CG_IsTrailSystemValid( trailSystem_t **ts );

void                CG_AddTrails( void );

void                CG_TestTS_f( void );
void                CG_DestroyTestTS_f( void );

//
// cg_ptr.c
//
int   CG_ReadPTRCode( void );
void  CG_WritePTRCode( int code );

//
// cg_tutorial.c
//
const char *CG_TutorialText( void );

// cg_main.c
bool    CG_GetRangeMarkerPreferences( bool *drawSurface, bool *drawIntersection,
                                          bool *drawFrontline, float *surfaceOpacity,
                                          float *lineOpacity, float *lineThickness );
// cg_drawtools.c
void        CG_DrawRangeMarker( rangeMarkerType_t rmType, const vec3_t origin, const float *angles, float range,
                                bool drawSurface, bool drawIntersection, bool drawFrontline,
                                const vec3_t rgb, float surfaceOpacity, float lineOpacity, float lineThickness );
// cg_buildable.c
bool    CG_GetBuildableRangeMarkerProperties( buildable_t bType, rangeMarkerType_t *rmType, float *range, vec3_t rgb );
void        CG_GhostBuildableRangeMarker( buildable_t buildable, const vec3_t origin, const vec3_t normal );
// cg_ents.c
void        CG_RangeMarker( centity_t *cent );

//
//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//
//
int trap_CM_MarkFragments( int numPoints, const vec3_t *points,
        const vec3_t projection,
        int maxPoints, vec3_t pointBuffer,
        int maxFragments, markFragment_t *fragmentBuffer );

// cg_drawCrosshair settings
#define CROSSHAIR_ALWAYSOFF       0
#define CROSSHAIR_RANGEDONLY      1
#define CROSSHAIR_ALWAYSON        2

// menu types for cg_disable*Dialogs
enum dialogType_t
{
  DT_INTERACTIVE, // team, klass, armoury
  DT_ARMOURYEVOLVE, // Insufficient funds et al
  DT_BUILD, // build errors
  DT_COMMAND, // You must be living/human/spec etc.

};
