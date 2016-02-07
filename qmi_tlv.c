
struct qmi_tlv *qmi_tlv_init(void)
{
	struct qmi_tlv *tlv;

	tlv = malloc(sizeof(struct qmi_tlv));
	memset(tlv, 0, sizeof(struct qmi_tlv));

	return tlv;
}

struct qmi_tlv *qmi_tlv_decode(void *buf, size_t len)
{
	struct qmi_tlv *tlv;

	tlv = malloc(sizeof(struct qmi_tlv));
	memset(tlv, 0, sizeof(struct qmi_tlv));

	tlv->buf = buf;
	tlv->size = len;

	return tlv;
}

void *qmi_tlv_encode(struct qmi_tlv *tlv, size_t *len)
{
	*len = tlv->size;
	return tlv->buf;
}

void qmi_tlv_free(struct qmi_tlv *tlv)
{
	free(tlv->allocated);
	free(tlv);
}

static struct qmi_tlv_header *qmi_tlv_get_item(struct qmi_tlv *tlv, unsigned id)
{
	struct qmi_tlv_header *hdr;
	unsigned offset = 0;

	while (offset < tlv->size) {
		hdr = tlv->buf + offset;
		if (hdr->key == id)
			return tlv->buf + offset;

		offset += sizeof(struct qmi_tlv_header) + hdr->len;
	}
	return NULL;
}

void *qmi_tlv_get(struct qmi_tlv *tlv, unsigned id, size_t *len)
{
	struct qmi_tlv_header *hdr;

	hdr = qmi_tlv_get_item(tlv, id);
	if (!hdr)
		return NULL;

	*len = hdr->len;
	return hdr->data;
}

void *qmi_tlv_get_array(struct qmi_tlv *tlv, unsigned id, size_t *len, size_t *size)
{
	struct qmi_tlv_header *hdr;
	uint8_t count;
	void *ptr;

	hdr = qmi_tlv_get_item(tlv, id);
	if (!hdr)
		return NULL;

	ptr = hdr->data;
	count = *(uint8_t*)ptr++;

	*len = count;
	*size = (hdr->len - 1) / count;

	return ptr;
}

static struct qmi_tlv_header *qmi_tlv_alloc_item(struct qmi_tlv *tlv, unsigned id, size_t len)
{
	struct qmi_tlv_header *hdr;
	size_t new_size;
	bool migrate;
	void *newp;

	/* If using user provided buffer, migrate data */
	migrate = !tlv->allocated;

	new_size = tlv->size + sizeof(struct qmi_tlv_header) + len;
	newp = realloc(tlv->allocated, new_size);
	if (!newp)
		return NULL;

	if (migrate)
		memcpy(newp, tlv->buf, tlv->size);

	hdr = newp + tlv->size;
	hdr->key = id;
	hdr->len = len;

	tlv->buf = tlv->allocated = newp;
	tlv->size = new_size;

	return hdr;
}

int qmi_tlv_set(struct qmi_tlv *tlv, unsigned id, void *buf, size_t len)
{
	struct qmi_tlv_header *hdr;

	hdr = qmi_tlv_alloc_item(tlv, id, len);
	if (!hdr)
		return -ENOMEM;

	memcpy(hdr->data, buf, len);

	return 0;
}

int qmi_tlv_set_array(struct qmi_tlv *tlv, unsigned id, void *buf, size_t len, size_t size)
{
	struct qmi_tlv_header *hdr;
	size_t array_size;
	void *ptr;

	array_size = len * size;
	hdr = qmi_tlv_alloc_item(tlv, id, sizeof(uint8_t) + array_size);
	if (!hdr)
		return -ENOMEM;

	ptr = hdr->data;
	*(uint8_t*)ptr++ = len;
	memcpy(ptr, buf, array_size);

	return 0;
}


