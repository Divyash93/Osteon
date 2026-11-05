extends SceneTree
## Renders a skeleton with Osteon2DSprite and saves a PNG (needs a window, so run it without --headless).
##   godot --path runtimes/godot --script res://tests/render_test.gd -- <skeleton.json> <atlas> <animation> <time> <out.png>
func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	var sprite := Osteon2DSprite.new()
	sprite.skeleton_path = a[0]
	sprite.atlas_path = a[1]
	sprite.position = Vector2(450, 700)
	sprite.scale = Vector2(0.45, 0.45)
	sprite.playing = false
	root.add_child(sprite)
	sprite.reload()
	sprite.play(a[2], true)
	sprite.advance(0.0)
	sprite.advance(float(a[3]))
	# frame the pose: fit the drawn points into the window
	var lo := Vector2(1e9, 1e9)
	var hi := Vector2(-1e9, -1e9)
	for b in sprite._builder.batches:
		for pt in b.points:
			lo = lo.min(Vector2(pt.x, -pt.y))
			hi = hi.max(Vector2(pt.x, -pt.y))
	var size := hi - lo
	var k := minf(860.0 / size.x, 760.0 / size.y)
	sprite.scale = Vector2(k, k)
	sprite.position = Vector2(450, 400) - (lo + size / 2.0) * k
	await process_frame
	await process_frame
	await process_frame
	var img := root.get_texture().get_image()
	img.save_png(a[4])
	print("saved ", a[4], " ", img.get_size())
	quit()
