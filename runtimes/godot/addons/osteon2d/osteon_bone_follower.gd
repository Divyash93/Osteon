@tool
class_name Osteon2DBoneFollower
extends Node2D
## Makes this node follow a bone of an Osteon2DSprite (weapon in hand, particles on a hip ...). Place it as a sibling or child
## of the sprite and assign the sprite and the bone name.

@export var sprite_path: NodePath:
	set(v):
		sprite_path = v
		_connect()
@export var bone_name := ""
@export var follow_position := true
@export var follow_rotation := true
@export var follow_scale := false
## Offset in the bone's own space, in skeleton units.
@export var local_offset := Vector2.ZERO
## Extra rotation in degrees.
@export var rotation_offset_degrees := 0.0

var _sprite: Osteon2DSprite = null


func _ready() -> void:
	_connect()


func _connect() -> void:
	if _sprite != null and _sprite.world_updated.is_connected(_apply):
		_sprite.world_updated.disconnect(_apply)
	_sprite = get_node_or_null(sprite_path) as Osteon2DSprite
	if _sprite != null:
		_sprite.world_updated.connect(_apply)


func _apply() -> void:
	if _sprite == null or _sprite.skeleton == null:
		return
	var sk := _sprite.skeleton
	var i := _sprite.data.find_bone(bone_name)
	if i < 0:
		return
	var b := sk.bones[i]
	var f := Vector2(-1.0 if _sprite.flip_h else 1.0, -1.0 if _sprite.flip_v else 1.0)
	if follow_position:
		var w := sk.local_to_world(b, local_offset.x, local_offset.y)
		global_position = _sprite.to_global(Vector2(w.x, -w.y) * f)
	if follow_rotation:
		var ang := Vector2(b.a * f.x, -b.c * f.y).angle() + deg_to_rad(rotation_offset_degrees)
		global_rotation = _sprite.global_rotation + ang
	if follow_scale:
		scale = Vector2(sqrt(b.a * b.a + b.c * b.c), sqrt(b.b * b.b + b.d * b.d))
