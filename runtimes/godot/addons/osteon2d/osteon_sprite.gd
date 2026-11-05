@tool
class_name Osteon2DSprite
extends Node2D
## Plays a skeleton exported from Osteon2D (Spine 4.2 JSON or an .osteon project, plus its libGDX .atlas and page images).
##
## Set `skeleton_path` and `atlas_path`, pick a `skin` and an `animation`, or drive it from code with play(), add_animation()
## and set_mix(). Bones are exposed through get_bone_position() and the Osteon2DBoneFollower node.

enum UpdateMode { PROCESS, PHYSICS, MANUAL }

## Emitted for every animation event the playhead passes.
signal animation_event(track: int, event_name: String, int_value: int, float_value: float, string_value: String)
## Emitted when a non looping animation reached its end.
signal animation_completed(track: int, animation_name: String)
## Emitted after the pose and world transforms were updated, before drawing. Use it to move bones.
signal world_updated
## Emitted after the skeleton was (re)loaded.
signal skeleton_loaded

@export_file("*.json", "*.osteon") var skeleton_path := "":
	set(v):
		skeleton_path = v
		if is_inside_tree():
			reload()
@export_file("*.atlas") var atlas_path := "":
	set(v):
		atlas_path = v
		if is_inside_tree():
			reload()
@export var skin := "default":
	set(v):
		skin = v
		if _skeleton != null:
			set_skin(v)
@export var animation := "":
	set(v):
		animation = v
		if _state != null and v != "":
			play(v, loop)
@export var loop := true
@export var playing := true
@export var speed_scale := 1.0
@export var update_mode: UpdateMode = UpdateMode.PROCESS
@export var flip_h := false
@export var flip_v := false
@export var clipping := true
## Draw bone lines in the editor and the running game (debugging).
@export var debug_bones := false:
	set(v):
		debug_bones = v
		queue_redraw()

var data: OsteonData = null
var atlas: OsteonAtlas = null
var skeleton: OsteonSkeleton = null
var _skeleton: OsteonSkeleton = null
var _state: OsteonState = null
var _builder: OsteonMeshBuilder = null
var _batch_nodes: Array[OsteonBatchNode] = []
var _materials := {}
var _overrides := {}
var _completed := {}  # entry -> true once animation_completed was emitted


func _ready() -> void:
	reload()


## (Re)loads the skeleton and atlas from the paths and starts the initial animation.
func reload() -> void:
	data = null
	atlas = null
	_skeleton = null
	skeleton = null
	_state = null
	_completed.clear()
	_clear_batches()
	if skeleton_path == "" or atlas_path == "":
		return
	if not FileAccess.file_exists(skeleton_path) or not FileAccess.file_exists(atlas_path):
		push_warning("Osteon2D: skeleton or atlas file not found (%s, %s). Exported games must include *.osteon, *.json and *.atlas in the export filters." % [skeleton_path, atlas_path])
		return
	data = OsteonData.from_json(FileAccess.get_file_as_string(skeleton_path))
	if data == null:
		return
	for w in data.warnings:
		push_warning("Osteon2D: " + w)
	atlas = OsteonAtlas.parse(FileAccess.get_file_as_string(atlas_path))
	atlas.load_textures(atlas_path)
	_skeleton = OsteonSkeleton.new(data)
	skeleton = _skeleton
	_apply_flip()
	if skin != "" and data.find_skin(skin) >= 0:
		_skeleton.set_skin_by_name(skin)
	_state = OsteonState.new(data)
	_state.event.connect(_on_state_event)
	_builder = OsteonMeshBuilder.new(atlas)
	if animation != "" and data.find_animation(animation) >= 0:
		_state.set_animation_by_name(0, animation, loop)
	advance(0.0)
	skeleton_loaded.emit()


func _on_state_event(track: int, ev: OsteonAnim.FiredEvent) -> void:
	animation_event.emit(track, ev.name, ev.int_value, ev.float_value, ev.string_value)


func _process(delta: float) -> void:
	if update_mode == UpdateMode.PROCESS and playing:
		advance(delta)


func _physics_process(delta: float) -> void:
	if update_mode == UpdateMode.PHYSICS and playing:
		advance(delta)


## Advances the animation by `delta` seconds and refreshes what is drawn.
func advance(delta: float) -> void:
	if _skeleton == null:
		return
	_state.update(delta * speed_scale)
	_skeleton.set_bones_to_setup_pose()
	_skeleton.set_slots_to_setup_pose()
	_state.apply(_skeleton)
	for slot_index in _overrides:
		_skeleton.slots[slot_index].attachment = _overrides[slot_index]
	_skeleton.update_world_transform()
	_check_completed()
	world_updated.emit()
	_rebuild()


func _check_completed() -> void:
	for ti in _state.tracks.size():
		var e: OsteonState.Entry = _state.tracks[ti]
		if e == null or e.loop or e.animation < 0:
			continue
		if e.track_time >= data.animations[e.animation].duration and not _completed.has(e):
			_completed[e] = true
			animation_completed.emit(ti, data.animations[e.animation].name)


# ---- API ----

## Plays an animation on a track, crossfading from what was playing there.
func play(animation_name: String, loop_animation := true, track := 0) -> OsteonState.Entry:
	if _state == null:
		return null
	playing = true
	return _state.set_animation_by_name(track, animation_name, loop_animation)

## Queues an animation after the current one on the track. A delay of 0 starts it when the current one ends.
func add_animation(animation_name: String, loop_animation := true, delay := 0.0, track := 0) -> OsteonState.Entry:
	if _state == null:
		return null
	return _state.add_animation_by_name(track, animation_name, loop_animation, delay)

func stop(track := 0) -> void:
	if _state != null:
		_state.clear_track(track)

## Crossfade duration in seconds between two animations (applies when switching from `from_name` to `to_name`).
func set_mix(from_name: String, to_name: String, duration: float) -> void:
	if _state != null:
		_state.set_mix(from_name, to_name, duration)

func set_default_mix(duration: float) -> void:
	if _state != null:
		_state.default_mix = duration

func get_animation_names() -> PackedStringArray:
	var out := PackedStringArray()
	if data != null:
		for a in data.animations:
			out.append(a.name)
	return out

func get_skin_names() -> PackedStringArray:
	var out := PackedStringArray()
	if data != null:
		for s in data.skins:
			out.append(s.name)
	return out

func get_bone_names() -> PackedStringArray:
	var out := PackedStringArray()
	if data != null:
		for b in data.bones:
			out.append(b.name)
	return out

func set_skin(skin_name: String) -> void:
	if _skeleton != null and _skeleton.set_skin_by_name(skin_name):
		skin = skin_name

## Forces a slot's attachment. An empty name clears the override and lets the animation decide again.
func set_attachment(slot_name: String, attachment_name: String) -> void:
	if _skeleton == null:
		return
	var i := data.find_slot(slot_name)
	if i < 0:
		push_error("Osteon2D: slot not found: " + slot_name)
		return
	if attachment_name == "":
		_overrides.erase(i)
	else:
		_overrides[i] = attachment_name

## Position of a bone's origin in this node's local space.
func get_bone_position(bone_name: String) -> Vector2:
	if _skeleton == null:
		return Vector2.ZERO
	var i := data.find_bone(bone_name)
	if i < 0:
		return Vector2.ZERO
	var b := _skeleton.bones[i]
	return Vector2(b.world_x, -b.world_y) * _flip_scale()

## World rotation of a bone in radians, in this node's local space (y down).
func get_bone_rotation(bone_name: String) -> float:
	if _skeleton == null:
		return 0.0
	var i := data.find_bone(bone_name)
	if i < 0:
		return 0.0
	var b := _skeleton.bones[i]
	var f := _flip_scale()
	return Vector2(b.a * f.x, -b.c * f.y).angle()

func _flip_scale() -> Vector2:
	return Vector2(-1.0 if flip_h else 1.0, -1.0 if flip_v else 1.0)

func _apply_flip() -> void:
	pass


# ---- drawing ----

func _clear_batches() -> void:
	for n in _batch_nodes:
		if is_instance_valid(n):
			n.queue_free()
	_batch_nodes.clear()


func _material_for(blend: int) -> CanvasItemMaterial:
	if _materials.has(blend):
		return _materials[blend]
	var m := CanvasItemMaterial.new()
	match blend:
		OsteonData.BlendMode.ADDITIVE, OsteonData.BlendMode.SCREEN:  # screen is approximated by additive blending
			m.blend_mode = CanvasItemMaterial.BLEND_MODE_ADD
		OsteonData.BlendMode.MULTIPLY:
			m.blend_mode = CanvasItemMaterial.BLEND_MODE_MUL
		_:
			m.blend_mode = CanvasItemMaterial.BLEND_MODE_MIX
	_materials[blend] = m
	return m


func _rebuild() -> void:
	_builder.use_clipping = clipping
	_builder.build(_skeleton)
	var f := _flip_scale()
	while _batch_nodes.size() < _builder.batches.size():
		var n := OsteonBatchNode.new()
		add_child(n, false, Node.INTERNAL_MODE_FRONT)
		_batch_nodes.append(n)
	for i in _batch_nodes.size():
		var n := _batch_nodes[i]
		if i < _builder.batches.size():
			var b := _builder.batches[i]
			n.batch = b
			n.texture = atlas.pages[b.page].texture
			n.material = _material_for(b.blend)
			n.scale = Vector2(f.x, -f.y)  # skeleton space has y up
			n.visible = true
			n.queue_redraw()
		else:
			n.batch = null
			n.visible = false
	if debug_bones:
		queue_redraw()


func _draw() -> void:
	if not debug_bones or _skeleton == null:
		return
	var f := _flip_scale()
	for b in _skeleton.bones:
		var from := Vector2(b.world_x, -b.world_y) * f
		var length := data.bones[b.index].length
		var tip := Vector2(b.world_x + b.a * length, -(b.world_y + b.c * length)) * f
		draw_line(from, tip, Color(1, 0.4, 0.2, 0.9), 1.5)
		draw_circle(from, 2.5, Color(0.2, 0.8, 1.0, 0.9))
