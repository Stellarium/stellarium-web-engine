// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<div style="height: 100%">
  <v-toolbar dark dense>
    <v-btn icon @click.stop.native="back">
      <v-icon>arrow_back</v-icon>
    </v-btn>
    <v-spacer></v-spacer>
  </v-toolbar>

  <v-tabs dark v-model="active" style="height: 0px;">
  </v-tabs>
  <v-tabs-items v-model="active">

    <v-tab-item key="0" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <div>
              <h3 class="headline mb-0">My Profile</h3>
            </div>
          </v-card-title>
          <v-card-text>
            <form action="">
            <v-layout row justify-space-between>
              <v-text-field label="First Name" disabled v-model="$store.state.noctuaSky.user.first_name"></v-text-field>
              &nbsp;
              <v-text-field label="Last Name" disabled v-model="$store.state.noctuaSky.user.last_name"></v-text-field>
              <v-btn icon ripple @click.native="showUpdateUserInfoTab()">
                <v-icon class="grey--text text--lighten-1">edit</v-icon>
              </v-btn>
            </v-layout>
            <v-text-field name="input-email" disabled label="Email" v-model="$store.state.noctuaSky.user.email"></v-text-field>
            <v-layout row justify-space-between>
              <v-text-field label="Password" disabled type="password" value="xxxxxxxx" autocomplete=""></v-text-field>
              <v-btn icon ripple @click.native="showChangePasswordTab()">
                <v-icon class="grey--text text--lighten-1">edit</v-icon>
              </v-btn>
            </v-layout>
            </form>
          </v-card-text>
          <v-card-actions>
            <v-spacer></v-spacer>
            <v-dialog v-model="showDeleteAccountDialog" width="600">
              <v-btn slot="activator" small class="red--text darken-1">Delete Account</v-btn>
              <v-card style="height: 280px" class="secondary white--text">
                <v-card-title primary-title>
                  <div>
                    <h3 class="headline mb-0">Delete my account</h3>
                  </div>
                </v-card-title>
                <v-card-text style="width:100%;">
                  Deleting your NoctuaSky account is permanent and cannot be undone. Before deleting your account, please keep in mind that:
                  <ul>
                    <li>Deleting your account will irremediably delete all observations, personal informations and pictures you created from our servers.</li>
                    <li>Observations you shared with public links won't be accessible anymore.</li>
                  </ul>
                </v-card-text>
                <v-card-actions>
                  <v-spacer></v-spacer>
                    <v-btn class="red--text darken-1" flat @click.native="deleteAccount()">Yes, delete all</v-btn>
                    <v-btn class="blue--text darken-1" flat @click.native="showDeleteAccountDialog = false">Cancel!</v-btn>
                </v-card-actions>
              </v-card>
            </v-dialog>
            <v-spacer></v-spacer>
          </v-card-actions>
        </v-card>
      </v-container>
    </v-tab-item>

    <v-tab-item key="1" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-card>
          <v-card-title primary-title>
            <div>
              <h3 class="headline mb-0">Change Password</h3>
            </div>
          </v-card-title>
          <v-progress-circular v-if="modifyPasswordInProgress" indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 60px;"></v-progress-circular>
          <v-card-text>
            <form action="">
            <v-text-field name="username" label="Email" disabled :value="$store.state.noctuaSky.user.email" type="text" autocomplete="username"></v-text-field>
            <v-text-field required name="input-oldpwd" label="Current password" autocomplete="current-password" v-model="currentPassword" type="password"></v-text-field>
            <v-text-field required name="input-newpwd" label="New password" autocomplete="new-password" v-model="newPassword" type="password" counter :rules="[rules.password]"></v-text-field>
            <v-text-field required name="input-newpwd2" label="Confirm new password" autocomplete="new-password" v-model="newPassword2" type="password" :rules="[rules.confirmPassword]"></v-text-field>
            </form>
            <v-alert error v-model="showChangePasswordErrorAlert">{{ changePasswordErrorAlert }}</v-alert>
            <v-layout row justify-space-around>
              <v-btn @click.stop.native="changePassword()" :disabled="!formValid">Send</v-btn>
            </v-layout>
          </v-card-text>
        </v-card>
      </v-container>
    </v-tab-item>

  <v-tab-item key="2" style="height: 100%">
    <v-container fluid style="height: 100%">
      <v-card>
        <v-card-title primary-title>
          <div>
            <h3 class="headline mb-0">Update User Information</h3>
          </div>
        </v-card-title>
        <v-progress-circular v-if="updateUserInfoInProgress" indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 60px;"></v-progress-circular>
        <v-card-text>
          <v-layout row justify-space-between>
            <v-text-field name="input-first-name" label="First Name" v-model="firstName"></v-text-field>
            &nbsp;
            <v-text-field name="input-last-name" label="Last Name" v-model="lastName"></v-text-field>
          </v-layout>
          <v-alert error v-model="showUserUpdateInfoErrorAlert">{{ updateUserInfoErrorAlert }}</v-alert>
          <v-layout row justify-space-around>
            <v-btn @click.stop.native="updateUserInfo()">Send</v-btn>
          </v-layout>
        </v-card-text>
      </v-card>
    </v-container>
  </v-tab-item>

  </v-tabs-items>
</div>
</template>

<script>
import NoctuaSkyClient from '@/assets/noctuasky-client'

export default {
  data: function () {
    return {
      active: '0',

      showDeleteAccountDialog: false,

      modifyPasswordInProgress: false,
      changePasswordErrorAlert: '',
      currentPassword: '',
      newPassword: '',
      newPassword2: '',

      updateUserInfoInProgress: false,
      updateUserInfoErrorAlert: '',
      firstName: '',
      lastName: '',

      rules: {
        password: (value) => {
          if (value === '') {
            return true
          }
          if (value.length < 8) {
            return 'Use at least 8 characters'
          }
          return true
        },
        confirmPassword: (value) => {
          if (value === '') {
            return true
          }
          if (value !== this.newPassword) {
            return 'Passwords don\'t match'
          }
          return true
        },
        email: (value) => {
          if (value === '') {
            return true
          }
          const pattern = /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/
          return pattern.test(value) || 'Invalid e-mail.'
        }
      }
    }
  },
  mounted: function () {
    this.active = '0'
  },
  methods: {
    showProfileTab: function () {
      this.active = '0'
    },
    showChangePasswordTab: function () {
      this.changePasswordErrorAlert = ''
      this.currentPassword = ''
      this.newPassword = ''
      this.newPassword2 = ''
      this.active = '1'
    },
    showUpdateUserInfoTab: function () {
      this.firstName = this.$store.state.noctuaSky.user.first_name
      this.lastName = this.$store.state.noctuaSky.user.last_name
      this.active = '2'
    },
    clearUserUpdateErrorAlert: function () {
      this.userUpdateErrorAlert = ''
    },
    back: function () {
      if (this.active === '1' || this.active === '2') {
        this.showProfileTab()
        return
      }
      this.$router.go(-1)
    },
    deleteAccount: function () {
      var that = this
      NoctuaSkyClient.users.deleteAccount().then(res => {
        that.showDeleteAccountDialog = false
        that.back()
      })
    },
    changePassword: function () {
      var that = this
      this.modifyPasswordInProgress = true
      NoctuaSkyClient.users.changePassword(this.currentPassword, this.newPassword).then(res => {
        that.back()
        that.modifyPasswordInProgress = false
      }).catch(function (res) {
        that.modifyPasswordInProgress = false
        if (res.message) {
          that.changePasswordErrorAlert = res.message
        } else if (res.errors) {
          that.changePasswordErrorAlert = ''
          for (let k in res.errors) {
            that.changePasswordErrorAlert += k + ': ' + res.errors[k] + '\n'
          }
        }
      })
    },
    updateUserInfo: function () {
      var that = this
      this.updateUserInfoInProgress = true
      NoctuaSkyClient.users.updateUserInfo({first_name: this.firstName, last_name: this.lastName}).then(res => {
        that.back()
        that.updateUserInfoInProgress = false
      }).catch(function (res) {
        that.updateUserInfoInProgress = false
        if (res.message) {
          that.updateUserInfoErrorAlert = res.message
        } else if (res.errors) {
          that.updateUserInfoErrorAlert = ''
          for (let k in res.errors) {
            that.updateUserInfoErrorAlert += k + ': ' + res.errors[k] + '\n'
          }
        }
      })
    }
  },
  computed: {
    formValid: function () {
      return this.currentPassword !== '' && this.newPassword.length > 5 && this.newPassword === this.newPassword2
    },
    showUserUpdateInfoErrorAlert: function () {
      return this.updateUserInfoErrorAlert !== ''
    },
    showChangePasswordErrorAlert: function () {
      return this.changePasswordErrorAlert !== ''
    }
  }
}
</script>

<style>
.tabs {
  height: 100%;
}
.tabs__items {
  height: 100%;
  border-style: none;
}
.tabs__bar {
  background-color: #303030;
  opacity: 0;
}
ul {
padding-left: 30px;
}
</style>
