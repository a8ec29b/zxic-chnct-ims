#ifndef IMS_REGISTRATION_H
#define IMS_REGISTRATION_H

#include "ims_register.h"
#include "ims_transport.h"

enum ims_reg_event {
    IMS_REG_ERROR = -1, IMS_REG_IGNORE, IMS_REG_SEND_CLEAR, IMS_REG_SEND_PROTECTED, IMS_REG_REGISTERED
};
enum ims_reg_state { IMS_REG_INITIAL, IMS_REG_RESYNC, IMS_REG_AUTHENTICATED, IMS_REG_READY, IMS_REG_FAILED };
enum ims_reg_failure {
    IMS_REG_FAILURE_RESPONSE, IMS_REG_FAILURE_CHALLENGE, IMS_REG_FAILURE_SECURITY,
    IMS_REG_FAILURE_AKA, IMS_REG_FAILURE_BUILD
};

typedef int (*ims_reg_aka_fn)(void *context, const unsigned char rand[16],
                              const unsigned char autn[16], struct usim_aka_result *result);

struct ims_registration {
    struct ims_register_request request;
    struct ims_challenge challenge;
    struct usim_aka_result aka;
    struct ims_security_parameters server;
    struct ims_transport transport;
    struct sip_slice verify;
    unsigned char local_address[16], remote_address[16];
    char security_verify[2049], branch_base[128], branch[160], service_route[1024];
    unsigned int expires_seconds;
    enum ims_reg_state state;
    enum ims_reg_failure failure;
    int resync_attempted;
};

/* The input strings remain owned by the caller for the lifetime of the flow. */
int ims_registration_init(struct ims_registration *r, const struct ims_register_request *request,
                          const unsigned char local[16], const unsigned char remote[16]);
int ims_registration_begin(struct ims_registration *r, char *packet, size_t capacity, size_t *length);
int ims_registration_refresh(struct ims_registration *r, char *packet, size_t capacity, size_t *length);
enum ims_reg_event ims_registration_response(struct ims_registration *r,
    const struct sip_message *response, int protected_transport, ims_reg_aka_fn authenticate,
    void *context, char *packet, size_t capacity, size_t *length);

#endif
