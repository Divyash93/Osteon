class_name OsteonAtlas
extends RefCounted
## Reader for the libGDX / Spine style .atlas text files that Osteon2D's texture packer writes.

class Page:
	var file := ""
	var w := 0
	var h := 0
	var texture: Texture2D = null

class AtlasRegion:
	var name := ""
	var page := 0
	var x := 0
	var y := 0
	var w := 0
	var h := 0                 # on the page; w,h are the unrotated size
	var rotated := false        # stored rotated 90 degrees clockwise (h x w on the page)
	var orig_w := 0
	var orig_h := 0
	var off_x := 0
	var off_y := 0
	var index := -1

	## Maps a point of the ORIGINAL image (u,v in 0..1, v pointing down) to page UVs (v pointing down).
	func page_uv(p: Page, u: float, v: float) -> Vector2:
		var ix := u * orig_w - off_x
		var iy := v * orig_h - (orig_h - off_y - h)
		var px: float
		var py: float
		if rotated:
			px = x + (h - iy)
			py = y + ix
		else:
			px = x + ix
			py = y + iy
		return Vector2(px / p.w, py / p.h)

var pages: Array[Page] = []
var regions: Array[AtlasRegion] = []
var _by_name := {}


func find(region_name: String) -> AtlasRegion:
	return _by_name.get(region_name)


static func _ints(v: String) -> PackedInt32Array:
	var out := PackedInt32Array()
	for part in v.split(","):
		out.append(part.strip_edges().to_int())
	return out


static func parse(text: String) -> OsteonAtlas:
	var atlas := OsteonAtlas.new()
	var need_page := true
	var cur: AtlasRegion = null
	for raw in text.split("\n"):
		var l := raw.strip_edges()
		if l == "":
			need_page = true
			cur = null
			continue
		var c := l.find(":")
		if c >= 0:
			var k := l.substr(0, c).strip_edges()
			var v := l.substr(c + 1).strip_edges()
			if cur != null:
				var n := _ints(v)
				if k == "bounds" and n.size() >= 4:
					cur.x = n[0]
					cur.y = n[1]
					cur.w = n[2]
					cur.h = n[3]
				elif k == "xy" and n.size() >= 2:
					cur.x = n[0]
					cur.y = n[1]
				elif k == "size" and n.size() >= 2:
					cur.w = n[0]
					cur.h = n[1]
				elif k == "offsets" and n.size() >= 4:
					cur.off_x = n[0]
					cur.off_y = n[1]
					cur.orig_w = n[2]
					cur.orig_h = n[3]
				elif k == "orig" and n.size() >= 2:
					cur.orig_w = n[0]
					cur.orig_h = n[1]
				elif k == "offset" and n.size() >= 2:
					cur.off_x = n[0]
					cur.off_y = n[1]
				elif k == "rotate":
					cur.rotated = v == "true" or v == "90"
				elif k == "index":
					cur.index = n[0] if n.size() > 0 else -1
			elif atlas.pages.size() > 0:
				var page := atlas.pages[atlas.pages.size() - 1]
				var n2 := _ints(v)
				if k == "size" and n2.size() >= 2:
					page.w = n2[0]
					page.h = n2[1]
			continue
		if need_page:
			var p := Page.new()
			p.file = l
			atlas.pages.append(p)
			need_page = false
			cur = null
		elif atlas.pages.size() > 0:
			cur = AtlasRegion.new()
			cur.name = l
			cur.page = atlas.pages.size() - 1
			atlas.regions.append(cur)
	for r in atlas.regions:
		if r.orig_w <= 0:
			r.orig_w = r.w
		if r.orig_h <= 0:
			r.orig_h = r.h
		var key := "%s_%d" % [r.name, r.index] if r.index >= 0 else r.name
		atlas._by_name[key] = r
		if not atlas._by_name.has(r.name):
			atlas._by_name[r.name] = r
	return atlas


## Loads the page images that sit next to the atlas file.
func load_textures(atlas_path: String) -> void:
	var dir := atlas_path.get_base_dir()
	for p in pages:
		var path := dir.path_join(p.file)
		if ResourceLoader.exists(path):
			p.texture = load(path)
		elif FileAccess.file_exists(path):
			var img := Image.load_from_file(path)
			if img != null:
				p.texture = ImageTexture.create_from_image(img)
		if p.texture == null:
			push_warning("Osteon2D: atlas page image not found: " + path)
		elif p.w <= 0:
			p.w = p.texture.get_width()
			p.h = p.texture.get_height()
