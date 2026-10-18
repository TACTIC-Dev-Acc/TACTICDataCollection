// Receives CombatData.csv uploads from TACTIC clients and stores each one as its own object
// in R2 — write-only by design: there is no GET/list route here, so even if the upload key ever
// leaked, the worst case is someone uploading junk files, not reading anyone else's data.
export default {
	async fetch(request, env) {
		if (request.method !== "PUT" && request.method !== "POST") {
			return new Response("Method not allowed", { status: 405 });
		}

		const providedKey = request.headers.get("X-Upload-Key");
		if (!providedKey || providedKey !== env.UPLOAD_KEY) {
			return new Response("Unauthorized", { status: 401 });
		}

		const url = new URL(request.url);
		const sessionId = url.searchParams.get("session") || "unknown-session";
		// Keep the object key predictable and safe regardless of what the client sends.
		const safeSessionId = sessionId.replace(/[^a-zA-Z0-9-]/g, "").slice(0, 64) || "unknown-session";
		const timestamp = new Date().toISOString().replace(/[:.]/g, "-");
		// Data-format version sent by the client (see kCsvVersion in UploadManager.cpp) becomes
		// the file name prefix, so CSVs from different versions stay separable. Absent for older
		// clients, which keep the unprefixed name they always had.
		const version = (url.searchParams.get("version") || "").replace(/[^a-zA-Z0-9_-]/g, "").slice(0, 32);
		const versionPrefix = version ? `${version}_` : "";
		// Clients that gzip the CSV before sending (encoding=gzip) get it stored as-is with a .csv.gz name,
		// so it downloads as a normal gzip file. Older clients send plain CSV and keep the .csv name.
		const gzipped = url.searchParams.get("encoding") === "gzip";
		const extension = gzipped ? "csv.gz" : "csv";
		const objectKey = `combat-data/${versionPrefix}${safeSessionId}_${timestamp}.${extension}`;

		await env.COMBAT_DATA_BUCKET.put(objectKey, request.body, {
			httpMetadata: { contentType: gzipped ? "application/gzip" : "text/csv" },
		});

		return new Response(`Stored as ${objectKey}`, { status: 200 });
	},
};
