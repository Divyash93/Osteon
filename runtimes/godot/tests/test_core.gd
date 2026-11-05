extends SceneTree
## Checks the GDScript runtime core against poses dumped from the C++ engine (osteon_golden).
##   godot --headless --path runtimes/godot --script res://tests/test_core.gd -- <skeleton.json> <golden.json>

var checks := 0
var failures := 0

func near(what: String, expect: float, got: float) -> void:
	checks += 1
	var tol := 2e-3 + 1e-4 * absf(expect)
	if absf(expect - got) > tol or is_nan(got):
		failures += 1
		if failures < 25:
			print("  MISMATCH %s: expected %s got %s" % [what, expect, got])

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var data := OsteonData.from_json(FileAccess.get_file_as_string(args[0]))
	if data == null:
		quit(2)
		return
	for w in data.warnings:
		print("warning: ", w)
	var golden = JSON.parse_string(FileAccess.get_file_as_string(args[1]))
	for sj in golden["samples"]:
		var label: String = sj["name"]
		var p := label.split(":")
		var sk := OsteonSkeleton.new(data)
		if p[0] == "apply":
			if p.size() > 3:
				sk.skin = maxi(0, data.find_skin(p[3]))
			sk.update_cache()
			sk.set_to_setup_pose()
			OsteonAnim.apply(data.animations[data.find_animation(p[1])], sk, -1.0, float(p[2]), false, [], 1.0, OsteonAnim.MixBlend.SETUP)
			sk.update_world_transform()
		else:
			if p.size() > 5:
				sk.skin = maxi(0, data.find_skin(p[5]))
			sk.update_cache()
			var st := OsteonState.new(data)
			st.set_animation(0, data.find_animation(p[1]), true)
			st.update(float(p[3]))
			sk.set_to_setup_pose()
			st.apply(sk)
			st.set_animation(0, data.find_animation(p[2]), true)
			st.update(float(p[4]))
			sk.set_to_setup_pose()
			st.apply(sk)
			sk.update_world_transform()
		var before := failures
		var bones: Array = sj["bones"]
		for i in bones.size():
			var m: Array = bones[i]
			var b := sk.bones[i]
			var got := [b.a, b.b, b.c, b.d, b.world_x, b.world_y]
			for k in 6:
				near("%s bone %s[%d]" % [label, data.bones[i].name, k], m[k], got[k])
		var order: Array = sj["drawOrder"]
		for i in order.size():
			checks += 1
			if int(order[i]) != sk.draw_order[i]:
				failures += 1
				print("  MISMATCH drawOrder %s at %d" % [label, i])
		for slj in sj["slots"]:
			var slot := sk.slots[int(slj["i"])]
			var col: Array = slj["color"]
			var gc := [slot.color.r, slot.color.g, slot.color.b, slot.color.a]
			for k in 4:
				near("%s slot %d color%d" % [label, slot.index, k], col[k], gc[k])
			if slj.has("verts"):
				var att := sk.attachment_for(slot)
				var v: Array = slj["verts"]
				checks += 1
				var got := sk.world_vertices(slot, att) if att != null else PackedFloat64Array()
				if got.size() != v.size():
					failures += 1
					print("  MISMATCH no vertices %s slot %d" % [label, slot.index])
					continue
				for k in v.size():
					near("%s slot %d vert%d" % [label, slot.index, k], v[k], got[k])
		print(("ok   " if failures == before else "FAIL ") + label)
	print("%d checks, %d mismatches" % [checks, failures])
	quit(0 if failures == 0 else 1)
