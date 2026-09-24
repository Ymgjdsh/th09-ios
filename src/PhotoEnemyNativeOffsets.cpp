// Native runtime bridge: file instructions remain 32-bit serialized records.
// Runtime addresses derive from the canonical compact TH095 owners, never
// retail x86 byte positions. These constants have constant initialization.
#ifdef TH095_IOS_PORTABLE_LAYOUT
#include "PhotoEnemyManager.hpp"
#include "EnemyManager.hpp"
namespace th095 {
const size_t PHOTO_ENEMY_ECL_MANAGER_OFFSET = offsetof(PhotoEnemyManagerView, eclManager);
const size_t PHOTO_ENEMY_ECL_PHOTO_TARGETS_OFFSET = offsetof(PhotoEnemyManagerView, photoTargets);
const size_t PHOTO_ENEMY_ECL_VM_ROTATION_Z_OFFSET = offsetof(PhotoEnemyView, vm)+offsetof(AnmVm, rotation)+offsetof(Float3, z);
const size_t PHOTO_ENEMY_ECL_ANM_HANDLES_OFFSET = offsetof(PhotoEnemyView, anmHandles);
const size_t PHOTO_ENEMY_ECL_ACTIVE_CONTEXT_OFFSET = offsetof(PhotoEnemyView, activeEclContext);
const size_t PHOTO_ENEMY_ECL_CONTEXT_FLOAT_VARIABLES_OFFSET = offsetof(PhotoEnemyEclContextView, scriptState)+offsetof(PhotoEnemyEclScriptStateView, floatVariables);
const size_t PHOTO_ENEMY_ECL_CONTEXT_EXTRA_FLOAT_VARIABLES_OFFSET = offsetof(PhotoEnemyEclContextView, scriptState)+offsetof(PhotoEnemyEclScriptStateView, extraFloatVariables);
const size_t PHOTO_ENEMY_ECL_CONTEXT_CALL_PARAMETER_FLOATS_OFFSET = offsetof(PhotoEnemyEclContextView, scriptState)+offsetof(PhotoEnemyEclScriptStateView, callParameterFloats);
const size_t PHOTO_ENEMY_ECL_PHOTO_CAPTURE_SUBROUTINE_OFFSET = offsetof(PhotoEnemyView, photoCaptureEclSubroutineId);
const size_t PHOTO_ENEMY_ECL_POSITION_OFFSET = offsetof(PhotoEnemyView, position);
const size_t PHOTO_ENEMY_ECL_MOVEMENT_ANGLE_OFFSET = offsetof(PhotoEnemyView, movementAngle);
const size_t PHOTO_ENEMY_ECL_ANGULAR_VELOCITY_OFFSET = offsetof(PhotoEnemyView, angularVelocity);
const size_t PHOTO_ENEMY_ECL_ORBIT_ANGLE_OFFSET = offsetof(PhotoEnemyView, orbitAngle);
const size_t PHOTO_ENEMY_ECL_ORBIT_ANGULAR_VELOCITY_OFFSET = offsetof(PhotoEnemyView, orbitAngularVelocity);
const size_t PHOTO_ENEMY_ECL_SPEED_OFFSET = offsetof(PhotoEnemyView, speed);
const size_t PHOTO_ENEMY_ECL_ACCELERATION_OFFSET = offsetof(PhotoEnemyView, acceleration);
const size_t PHOTO_ENEMY_ECL_ORBIT_RADIUS_OFFSET = offsetof(PhotoEnemyView, orbitRadius);
const size_t PHOTO_ENEMY_ECL_INTERPOLATION_DELTA_OFFSET = offsetof(PhotoEnemyView, movementInterpolationDelta);
const size_t PHOTO_ENEMY_ECL_INTERPOLATION_ORIGIN_OFFSET = offsetof(PhotoEnemyView, movementInterpolationOrigin);
const size_t PHOTO_ENEMY_ECL_LIFE_OFFSET = offsetof(PhotoEnemyView, life);
const size_t PHOTO_ENEMY_ECL_MAXIMUM_LIFE_OFFSET = offsetof(PhotoEnemyView, maximumLife);
const size_t PHOTO_ENEMY_ECL_PHASE_STARTING_LIFE_OFFSET = offsetof(PhotoEnemyView, phaseStartingLife);
const size_t PHOTO_ENEMY_ECL_SCORE_OFFSET = offsetof(PhotoEnemyView, score);
const size_t PHOTO_ENEMY_ECL_TIMER_OFFSET = offsetof(PhotoEnemyView, eclTimer);
const size_t PHOTO_ENEMY_ECL_TIMER_CURRENT_OFFSET = offsetof(PhotoEnemyView, eclTimer)+offsetof(ZunTimer, current);
const size_t PHOTO_ENEMY_ECL_BULLET_DESCRIPTOR_OFFSET = offsetof(PhotoEnemyView, bulletSpawnDescriptor);
const size_t PHOTO_ENEMY_ECL_BULLET_TRANSFORMS_OFFSET = offsetof(PhotoEnemyView, bulletSpawnDescriptor)+offsetof(PhotoBulletSpawnDescriptor, transforms);
const size_t PHOTO_ENEMY_ECL_BULLET_TRANSFORM_FLAGS_OFFSET = offsetof(PhotoEnemyView, bulletSpawnDescriptor)+offsetof(PhotoBulletSpawnDescriptor, transformFlags);
const size_t PHOTO_ENEMY_ECL_BULLET_SPAWN_SOUND_OFFSET = offsetof(PhotoEnemyView, bulletSpawnDescriptor)+offsetof(PhotoBulletSpawnDescriptor, spawnSound);
const size_t PHOTO_ENEMY_ECL_BULLET_TRANSFORM_SOUND_OFFSET = offsetof(PhotoEnemyView, bulletSpawnDescriptor)+offsetof(PhotoBulletSpawnDescriptor, transformSound);
const size_t PHOTO_ENEMY_ECL_PENDING_SHOT_OFFSET = offsetof(PhotoEnemyView, pendingShotInstruction);
const size_t PHOTO_ENEMY_ECL_SHOOT_INTERVAL_FRAMES_OFFSET = offsetof(PhotoEnemyView, shootIntervalFrames);
const size_t PHOTO_ENEMY_ECL_SHOOT_INTERVAL_TIMER_OFFSET = offsetof(PhotoEnemyView, shootIntervalTimer);
const size_t PHOTO_ENEMY_ECL_ITEM_DROP_TYPE_OFFSET = offsetof(PhotoEnemyView, itemDropType);
const size_t PHOTO_ENEMY_ECL_PHOTO_TARGET_SLOT_OFFSET = offsetof(PhotoEnemyView, photoTargetSlot);
const size_t PHOTO_ENEMY_ECL_CONTROL_OFFSET = offsetof(PhotoEnemyView, flags1);
const size_t PHOTO_ENEMY_ECL_SECONDARY_CONTROL_OFFSET = offsetof(PhotoEnemyView, flags2);
const size_t PHOTO_ENEMY_ECL_PHOTO_MARKER_TIMER_OFFSET = offsetof(PhotoEnemyView, photoMarkerPulseTimer);
const size_t PHOTO_ENEMY_ECL_DRAW_GROUP_OFFSET = offsetof(PhotoEnemyView, drawGroup);
const size_t PHOTO_ENEMY_ECL_PHOTO_PULSE_VM_OFFSET = offsetof(PhotoEnemyView, photoPulseVmId);
const size_t PHOTO_ENEMY_ECL_PHOTO_MARKER_VM_OFFSET = offsetof(PhotoEnemyView, photoMarkerVmId);
const size_t PHOTO_ENEMY_ECL_PHOTO_PULSE_TIMER_OFFSET = offsetof(PhotoEnemyView, photoPulseTimer);
const size_t PHOTO_ENEMY_ECL_PHOTO_PULSE_DURATION_TIMER_OFFSET = offsetof(PhotoEnemyView, photoPulseDurationTimer);
const size_t PHOTO_ENEMY_ECL_MOVEMENT_BOUNDS_OFFSET = offsetof(PhotoEnemyView, movementBoundsMin);
const size_t PHOTO_ENEMY_ECL_MINIMUM_PLAYER_DISTANCE_SQUARED_OFFSET = offsetof(PhotoEnemyView, minimumPlayerDistanceSquared);
const size_t PHOTO_ENEMY_ECL_UNKNOWN_2C50_OFFSET = offsetof(PhotoEnemyView, unknown2c50);
const size_t PHOTO_ENEMY_ECL_SCHEDULED_FRAMES_OFFSET = offsetof(PhotoEnemyView, scheduledCallFrames);
const size_t PHOTO_ENEMY_ECL_SCHEDULED_CALLS_OFFSET = offsetof(PhotoEnemyView, scheduledCalls);
const size_t PHOTO_ENEMY_ECL_PENDING_CALLBACK_FRAME_OFFSET = offsetof(PhotoEnemyView, pendingCallbackFrame);
const size_t PHOTO_ENEMY_ECL_PENDING_CALLBACK_SUBROUTINE_OFFSET = offsetof(PhotoEnemyView, unknown2ca8);
const size_t PHOTO_ENEMY_ECL_CHILD_BLOCKS_OFFSET = offsetof(PhotoEnemyView, childEclBlocks);
const size_t PHOTO_ENEMY_ECL_TRAIL_VERTICES_OFFSET = offsetof(PhotoEnemyView, trailVertices);
const size_t PHOTO_ENEMY_ECL_PHOTO_ANM_CONFIG_OFFSET = offsetof(PhotoEnemyView, unknown4ca4);
const size_t PHOTO_ENEMY_ECL_TIMER_4CAC_OFFSET = offsetof(PhotoEnemyView, timer4cac);
const size_t PHOTO_ENEMY_ECL_ATTACHED_VM_OFFSET = offsetof(PhotoEnemyView, attachedVmId);
static_assert(sizeof(EnemyEclContext) == sizeof(PhotoEnemyEclContextView), "ECL context extent");
static_assert(offsetof(EnemyEclContext, intVariables) == offsetof(PhotoEnemyEclContextView, scriptState), "ECL variables");
static_assert(offsetof(EnemyEclContext, interpolationSlots) == offsetof(PhotoEnemyEclContextView, interpolationSlots), "ECL interpolation");
static_assert(offsetof(Enemy, vm) == offsetof(PhotoEnemyView, vm), "Enemy VM prefix");
static_assert(offsetof(Enemy, mainEclContextStorage) == offsetof(PhotoEnemyView, mainEclContext), "Enemy context prefix");
static_assert(offsetof(Enemy, activeEclContext) == offsetof(PhotoEnemyView, activeEclContext), "Active context");
static_assert(offsetof(Enemy, position) == offsetof(PhotoEnemyView, position), "Enemy position");
static_assert(offsetof(Enemy, movementDuration) == offsetof(PhotoEnemyView, movementDuration), "Enemy movement prefix");
}
#endif
